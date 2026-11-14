#!/usr/bin/env python3
"""Build the headless simulator renderer and regenerate manual OLED screenshots.

Every catalog state is written twice: the screen variant (light on dark, as on
the module; used by README, wiki and simulator documentation) and the print
variant used by the user manual (dark on light, framed by the display cut-out
of the front panel).

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""

from __future__ import annotations

import argparse
import binascii
import configparser
import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "docs" / "manual-source" / "assets"
BUILD_DIR = ROOT / "build" / "simulator-headless"
GENERATOR_BASENAME = "clock-manual-screenshot-generator"
MANIFEST_NAME = "manual-screenshots.tsv"
PRESET_NAME = "simulator-headless"
PANEL_LAYOUT = ROOT / "sim" / "panel_layout.ini"
PANEL_ARTWORK = ROOT / "docs" / "assets" / "front-panel" / "clock-front-panel.svg"

OLED_WIDTH_PX = 128
OLED_HEIGHT_PX = 64
PRINT_SUFFIX = "-print"
#: Integer nearest-neighbour enlargement of one OLED pixel in the print variant.
PRINT_PIXEL_SCALE = 8
# ASSUMPTION: size and position of the active OLED area inside the panel's
# display cut-out are not part of the KiCad data. Until the display module's
# drawing provides them, the active area is taken as 21.8 mm x 10.9 mm,
# centred in the cut-out (D-12 point 5).
OLED_ACTIVE_WIDTH_MM = 21.8
OLED_ACTIVE_HEIGHT_MM = 10.9
#: Printed size of a screenshot figure relative to the panel (D-12 point 10).
PRINT_FIGURE_SCALE = 2.2
#: Line width of the display cut-out outline, in panel millimetres.
PRINT_FRAME_STROKE_MM = 0.12
#: Supersampling factor used only to antialias the frame outline.
FRAME_SUPERSAMPLE = 4


def run(command: list[str]) -> None:
    """Execute one subprocess and fail immediately on errors."""
    print("+", " ".join(command))
    subprocess.run(command, cwd=ROOT, check=True)


def preset_generator() -> str:
    """Return the generator declared by the headless simulator configure preset."""
    presets = json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
    for preset in presets.get("configurePresets", []):
        if preset.get("name") == PRESET_NAME:
            generator = preset.get("generator")
            if not generator:
                raise RuntimeError(f"CMake preset {PRESET_NAME!r} does not declare a generator")
            return str(generator)
    raise RuntimeError(f"CMake configure preset {PRESET_NAME!r} was not found")


def configured_generator(build_dir: Path) -> str | None:
    """Read the generator recorded in an existing CMake build tree, if present."""
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        return None
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("CMAKE_GENERATOR:INTERNAL="):
            return line.split("=", 1)[1].strip()
    return None


def ensure_compatible_build_tree(build_dir: Path = BUILD_DIR) -> None:
    """Remove a stale headless build tree when it was configured with another generator."""
    current = configured_generator(build_dir)
    expected = preset_generator()
    if current is not None and current != expected:
        print(
            f"Removing incompatible CMake build tree {build_dir}: "
            f"configured with {current!r}, preset requires {expected!r}"
        )
        shutil.rmtree(build_dir)


def generator_path() -> Path:
    """Return the configured generator executable path for this host platform."""
    suffix = ".exe" if sys.platform.startswith("win") else ""
    return BUILD_DIR / f"{GENERATOR_BASENAME}{suffix}"


def read_pgm(path: Path) -> tuple[int, int, bytes]:
    """Read the generator's binary P5 grayscale image without third-party packages."""
    data = path.read_bytes()
    offset = 0

    def token() -> bytes:
        nonlocal offset
        while offset < len(data):
            if data[offset:offset + 1] == b"#":
                newline = data.find(b"\n", offset)
                if newline < 0:
                    raise ValueError(f"Malformed PGM comment in {path}")
                offset = newline + 1
                continue
            if not data[offset:offset + 1].isspace():
                break
            offset += 1
        start = offset
        while offset < len(data) and not data[offset:offset + 1].isspace():
            offset += 1
        return data[start:offset]

    if token() != b"P5":
        raise ValueError(f"Unsupported PGM format in {path}")
    width = int(token())
    height = int(token())
    maximum = int(token())
    if maximum != 255:
        raise ValueError(f"Unsupported PGM range in {path}: {maximum}")
    while offset < len(data) and data[offset:offset + 1].isspace():
        offset += 1
    pixels = data[offset:]
    expected = width * height
    if len(pixels) != expected:
        raise ValueError(f"PGM pixel length mismatch in {path}: {len(pixels)} != {expected}")
    return width, height, pixels


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    """Encode one PNG chunk with its CRC."""
    checksum = binascii.crc32(kind)
    checksum = binascii.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def write_scaled_png(path: Path, width: int, height: int, pixels: bytes, scale: int) -> None:
    """Write a grayscale PNG using integer nearest-neighbour scaling only."""
    scaled_width = width * scale
    scaled_height = height * scale
    raw = bytearray()
    for source_y in range(height):
        row = pixels[source_y * width:(source_y + 1) * width]
        scaled_row = b"".join(bytes((value,)) * scale for value in row)
        for _ in range(scale):
            raw.append(0)  # PNG filter type: None
            raw.extend(scaled_row)

    ihdr = struct.pack(">IIBBBBB", scaled_width, scaled_height, 8, 0, 0, 0, 0)
    encoded = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", zlib.compress(bytes(raw), level=9))
        + png_chunk(b"IEND", b"")
    )
    path.write_bytes(encoded)


def display_cutout() -> tuple[float, float, float]:
    """Return width, height and corner radius (mm) of the panel's display cut-out.

    Width and height come from ``sim/panel_layout.ini``; the corner radius is
    read from the cut-out's arc in the KiCad-derived panel artwork. Both files
    are written by ``scripts/import_kicad_front_panel.py``.
    """
    layout = configparser.ConfigParser(interpolation=None)
    layout.read(PANEL_LAYOUT, encoding="utf-8")
    x = layout.getfloat("display", "x_mm")
    y = layout.getfloat("display", "y_mm")
    width = layout.getfloat("display", "width_mm")
    height = layout.getfloat("display", "height_mm")
    artwork = PANEL_ARTWORK.read_text(encoding="utf-8")
    number = r"([0-9]+(?:\.[0-9]+)?)"
    for match in re.finditer(rf"M{number} {number} A{number} ", artwork):
        start_x, start_y, radius = (float(value) for value in match.groups())
        if abs(start_x - x) < 1e-3 and y < start_y < y + height:
            return width, height, radius
    raise RuntimeError(f"Display cut-out arc not found in {PANEL_ARTWORK}")


def print_variant_image(width: int, height: int, pixels: bytes):
    """Return the framed print variant of one OLED frame as a Pillow image.

    OLED pixels are inverted (lit = black) and enlarged by an integer
    nearest-neighbour factor without any other change. The frame is the
    display cut-out outline as a hairline; only the frame is antialiased.
    """
    from PIL import Image, ImageDraw

    if (width, height) != (OLED_WIDTH_PX, OLED_HEIGHT_PX):
        raise ValueError(f"Unexpected OLED dimensions: {width}x{height}")
    cutout_w, cutout_h, radius = display_cutout()
    px_per_mm = OLED_WIDTH_PX * PRINT_PIXEL_SCALE / OLED_ACTIVE_WIDTH_MM
    stroke = PRINT_FRAME_STROKE_MM * px_per_mm
    canvas_w = round((cutout_w + PRINT_FRAME_STROKE_MM) * px_per_mm)
    canvas_h = round((cutout_h + PRINT_FRAME_STROKE_MM) * px_per_mm)

    big = FRAME_SUPERSAMPLE
    frame = Image.new("L", (canvas_w * big, canvas_h * big), 255)
    inset = stroke * big / 2
    ImageDraw.Draw(frame).rounded_rectangle(
        (inset, inset, canvas_w * big - inset, canvas_h * big - inset),
        radius=radius * px_per_mm * big,
        outline=0,
        width=max(1, round(stroke * big)),
    )
    frame = frame.resize((canvas_w, canvas_h), Image.Resampling.LANCZOS)

    oled = Image.frombytes("L", (width, height), bytes(255 - value for value in pixels))
    oled = oled.resize(
        (width * PRINT_PIXEL_SCALE, height * PRINT_PIXEL_SCALE), Image.Resampling.NEAREST
    )
    frame.paste(oled, ((canvas_w - oled.width) // 2, (canvas_h - oled.height) // 2))
    return frame


def write_print_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    """Write the print variant; its DPI gives a natural size of 2.2 : 1 of the panel."""
    image = print_variant_image(width, height, pixels)
    px_per_mm = OLED_WIDTH_PX * PRINT_PIXEL_SCALE / OLED_ACTIVE_WIDTH_MM
    dpi = round(px_per_mm / PRINT_FIGURE_SCALE * 25.4, 3)
    image.save(path, format="PNG", optimize=True, dpi=(dpi, dpi))


def parse_manifest(path: Path) -> list[tuple[str, str]]:
    """Read the ordered screenshot catalog.

    The native generator writes ``name<TAB>description``; the published
    catalog adds the print variant: ``name<TAB>print name<TAB>description``.
    """
    entries: list[tuple[str, str]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        fields = line.split("\t")
        entries.append((fields[0], fields[-1]))
    return entries


def print_name(name: str) -> str:
    """Return the catalog name of the print variant of screenshot *name*."""
    return f"{name}{PRINT_SUFFIX}"


DISTINCT_SCREENSHOT_PAIRS = (
    ("performance-independent-clock-play", "performance-one-clock-play"),
    ("performance-independent-euclid-play", "performance-independent-clock-play"),
    ("performance-independent-sequencer-play", "performance-independent-clock-play"),
    ("channel-overview-independent", "channel-overview-one-clock"),
)


def validate_generated_catalog(directory: Path, entries: list[tuple[str, str]]) -> None:
    """Reject stale/degenerate screenshot catalogs before publishing manual assets."""
    names = [name for name, _ in entries]
    if len(names) != len(set(names)):
        raise RuntimeError("Manual screenshot generator emitted duplicate catalog names")
    available = set(names)
    for left, right in DISTINCT_SCREENSHOT_PAIRS:
        if left not in available or right not in available:
            raise RuntimeError(f"Manual screenshot catalog is missing required states: {left}, {right}")
        if (directory / f"{left}.pgm").read_bytes() == (directory / f"{right}.pgm").read_bytes():
            raise RuntimeError(
                "Manual screenshot renderer collapsed distinct UI states into an identical frame: "
                f"{left} == {right}"
            )


def remove_previous_outputs(output_dir: Path) -> None:
    """Delete only files listed in the previous generated manifest."""
    previous_manifest = output_dir / MANIFEST_NAME
    if not previous_manifest.exists():
        return
    for name, _ in parse_manifest(previous_manifest):
        for candidate in (output_dir / f"{name}.png", output_dir / f"{print_name(name)}.png"):
            if candidate.exists():
                candidate.unlink()


def generate(output_dir: Path, scale: int) -> int:
    """Build the generator, render the catalog, and publish deterministic PNG files."""
    if scale < 1:
        raise ValueError("scale must be at least 1")

    ensure_compatible_build_tree()
    run(["cmake", "--preset", PRESET_NAME])
    run(["cmake", "--build", "--preset", PRESET_NAME, "--target", GENERATOR_BASENAME])

    executable = generator_path()
    if not executable.exists():
        raise FileNotFoundError(f"Screenshot generator not found: {executable}")

    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="clock-manual-screenshots-") as temporary:
        temporary_dir = Path(temporary)
        run([str(executable), str(temporary_dir)])
        entries = parse_manifest(temporary_dir / "manifest.tsv")
        validate_generated_catalog(temporary_dir, entries)
        remove_previous_outputs(output_dir)

        for name, _ in entries:
            pgm_path = temporary_dir / f"{name}.pgm"
            width, height, pixels = read_pgm(pgm_path)
            if (width, height) != (128, 64):
                raise ValueError(f"Unexpected OLED dimensions for {name}: {width}x{height}")
            write_scaled_png(output_dir / f"{name}.png", width, height, pixels, scale)
            write_print_png(output_dir / f"{print_name(name)}.png", width, height, pixels)

        manifest_text = "".join(
            f"{name}\t{print_name(name)}\t{description}\n" for name, description in entries
        )
        (output_dir / MANIFEST_NAME).write_text(manifest_text, encoding="utf-8", newline="\n")

    try:
        shown_output = output_dir.relative_to(ROOT)
    except ValueError:
        shown_output = output_dir
    print(
        f"Generated {len(entries)} screenshots in {shown_output} at {scale}x nearest-neighbour scale, "
        f"each with a print variant at {PRINT_PIXEL_SCALE}x."
    )
    return len(entries)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="Destination directory (default: docs/manual-source/assets)",
    )
    parser.add_argument(
        "--scale",
        type=int,
        default=4,
        help="Integer nearest-neighbour enlargement factor (default: 4)",
    )
    args = parser.parse_args()
    generate(args.output.resolve(), args.scale)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
