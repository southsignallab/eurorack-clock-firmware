#!/usr/bin/env python3
# Author: Axel Napolitano
# License: PolyForm-Noncommercial-1.0.0
"""Generate the light and dark README hero images.

The layout follows a fixed 1280 x 600 CSS-pixel design rendered at twice that
size (2560 x 1200). The left column carries the product line, the wordmark, a
one-sentence description and a status line whose version is read from
src/version.h (CLOCK_FIRMWARE_VERSION). The right column shows the approved
Revision C render in docs/assets/readme/clock-approved-render-ui.webp.

Text is set in Ubuntu Sans and Ubuntu Sans Mono (variable fonts, Ubuntu Font
Licence 1.0) from third_party/fonts/ubuntu-sans/.

Each output WebP carries an EXIF ImageDescription naming the firmware version
and a SHA-256 fingerprint of every generator input. ``--check`` fails when the
version or the fingerprint of a committed image no longer matches, or when a
fresh render differs visibly from the committed pixels. The pixel comparison
uses a tolerance so that a different FreeType or libwebp build does not fail
the check by itself.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import re
import sys
from dataclasses import dataclass
from pathlib import Path

try:
    from PIL import Image, ImageChops, ImageDraw, ImageFont, ImageStat
except ImportError as exc:  # pragma: no cover - environment guard
    raise SystemExit("Pillow is required: python -m pip install pillow") from exc

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from version import VERSION_RE, read_define  # noqa: E402  (shared version source)

VERSION_HEADER = ROOT / "src" / "version.h"
FONT_DIR = ROOT / "third_party" / "fonts" / "ubuntu-sans"
SANS_FONT = FONT_DIR / "UbuntuSans[wdth,wght]-latin.woff2"
MONO_FONT = FONT_DIR / "UbuntuSansMono[wght]-latin.woff2"
RENDER = ROOT / "docs" / "assets" / "readme" / "clock-approved-render-ui.webp"
OUTPUT_DIR = ROOT / "docs" / "assets" / "readme"
OUTPUT_NAME = "clock-firmware-hero-{theme}.webp"

SCALE = 2                      # output pixels per CSS pixel
CANVAS_W_CSS = 1280
CANVAS_H_CSS = 600
WEBP_QUALITY = 90
STABLE_VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
DESCRIPTION_RE = re.compile(r"^CLOCK firmware README hero (?P<version>\S+) (?P<theme>light|dark) inputs=(?P<digest>[0-9a-f]{64})$")
EXIF_IMAGE_DESCRIPTION = 0x010E
MAX_MEAN_DIFFERENCE = 1.0      # mean absolute channel difference (0..255) tolerated by --check

# Ubuntu Sans vertical metrics in em (hhea ascender 940, descender 260 per 1000 units).
ASCENT_EM = 0.94
DESCENT_EM = 0.26
NORMAL_LINE_EM = ASCENT_EM + DESCENT_EM

EYEBROW = "Eurorack · 3U · 10 HP"
WORDMARK = "CLOCK"
SUBTITLE = "Firmware for the eight-channel master clock and gate sequencer by South Signal Lab"
PLATFORM = "STM32F401 · C++17"
CAPTION = "Hardware Revision C, rendered from the design files"


@dataclass(frozen=True)
class Theme:
    """Colours of one hero variant."""

    name: str
    background: str
    ink: str
    muted: str
    subtitle: str
    rule: str
    accent: str
    border: str | None


THEMES = (
    Theme("light", "#ffffff", "#000000", "#555555", "#222222", "#000000", "#0b4fc0", "#e3e3e3"),
    Theme("dark", "#000000", "#ffffff", "#b3b3b3", "#ffffff", "#ffffff", "#0b4fc0", None),
)


def firmware_version() -> str:
    """Return CLOCK_FIRMWARE_VERSION from src/version.h."""
    return read_define(VERSION_HEADER, VERSION_RE)


def status_text(version: str) -> str:
    """Return the status line text: released for X.Y.Z, otherwise pre-release."""
    state = "released" if STABLE_VERSION_RE.match(version) else "pre-release"
    return f"{version} · {state}"


def input_fingerprint(version: str, theme: str) -> str:
    """Hash every input that determines the hero image (line endings normalised)."""
    digest = hashlib.sha256()
    script = Path(__file__).read_bytes().replace(b"\r\n", b"\n")
    for part in (script, SANS_FONT.read_bytes(), MONO_FONT.read_bytes(), RENDER.read_bytes(),
                 version.encode("utf-8"), theme.encode("utf-8")):
        digest.update(hashlib.sha256(part).digest())
    return digest.hexdigest()


def description(version: str, theme: str) -> str:
    """Return the EXIF ImageDescription written into each output."""
    return f"CLOCK firmware README hero {version} {theme} inputs={input_fingerprint(version, theme)}"


def font(path: Path, size_css: float, axes: list[int]) -> ImageFont.FreeTypeFont:
    """Load a variable font at a CSS pixel size and select its axis values."""
    face = ImageFont.truetype(str(path), round(size_css * SCALE))
    face.set_variation_by_axes(axes)
    return face


def sans(size_css: float, weight: int) -> ImageFont.FreeTypeFont:
    return font(SANS_FONT, size_css, [100, weight])  # axes: wdth, wght


def mono(size_css: float) -> ImageFont.FreeTypeFont:
    return font(MONO_FONT, size_css, [400])  # axis: wght


def px(value_css: float) -> float:
    return value_css * SCALE


def draw_spaced(draw: ImageDraw.ImageDraw, x_css: float, baseline_css: float, text: str,
                face: ImageFont.FreeTypeFont, fill: str, spacing_css: float = 0.0) -> float:
    """Draw text on a baseline with CSS letter-spacing; return the advance in CSS px."""
    if spacing_css == 0.0:
        draw.text((px(x_css), px(baseline_css)), text, font=face, fill=fill, anchor="ls")
        return face.getlength(text) / SCALE
    for index, char in enumerate(text):
        offset = face.getlength(text[:index]) + index * px(spacing_css)
        draw.text((px(x_css) + offset, px(baseline_css)), char, font=face, fill=fill, anchor="ls")
    return (face.getlength(text) + len(text) * px(spacing_css)) / SCALE


def wrap(text: str, face: ImageFont.FreeTypeFont, width_css: float) -> list[str]:
    """Greedy word wrap at spaces (no-break spaces stay joined), like a browser line box."""
    lines: list[str] = []
    current = ""
    for word in text.split(" "):
        candidate = f"{current} {word}" if current else word
        if current and face.getlength(candidate) > px(width_css):
            lines.append(current)
            current = word
        else:
            current = candidate
    lines.append(current)
    return lines


def render(theme: Theme, version: str) -> Image.Image:
    """Render one hero variant at 2560 x 1200."""
    canvas = Image.new("RGB", (CANVAS_W_CSS * SCALE, CANVAS_H_CSS * SCALE), theme.background)
    draw = ImageDraw.Draw(canvas)
    hero_h = CANVAS_H_CSS
    if theme.border is not None:
        hero_h -= 1  # border-bottom: 1px inside a border-box hero
        draw.rectangle((0, px(hero_h), px(CANVAS_W_CSS) - 1, px(CANVAS_H_CSS) - 1), fill=theme.border)

    eyebrow_face, h1_face = mono(17), sans(132, 600)
    sub_face, status_face, cap_face = sans(29, 400), mono(18), sans(15, 400)

    # Left column: flex column, vertically centred (CSS values, line-height normal = 1.2 em).
    left_x = 72.0
    sub_lines = wrap(SUBTITLE, sub_face, 600)
    blocks = (
        17 * NORMAL_LINE_EM,                       # eyebrow
        18 + 132,                                  # h1, line-height 1
        20 + len(sub_lines) * 29 * 1.3,            # subtitle, line-height 1.3
        34 + 2,                                    # rule
        18 + 18 * NORMAL_LINE_EM,                  # status
    )
    top = (hero_h - sum(blocks)) / 2

    def baseline(line_top: float, size: float, line_height: float) -> float:
        return line_top + (line_height - size * NORMAL_LINE_EM) / 2 + size * ASCENT_EM

    draw_spaced(draw, left_x, baseline(top, 17, 17 * NORMAL_LINE_EM), EYEBROW.upper(), eyebrow_face, theme.muted, 0.06 * 17)
    top += blocks[0] + 18
    draw_spaced(draw, left_x, baseline(top, 132, 132), WORDMARK, h1_face, theme.ink, -0.01 * 132)
    top += 132 + 20
    for line in sub_lines:
        draw_spaced(draw, left_x, baseline(top, 29, 29 * 1.3), line, sub_face, theme.subtitle)
        top += 29 * 1.3
    top += 34
    draw.rectangle((px(left_x), px(top), px(left_x + 560) - 1, px(top + 2) - 1), fill=theme.rule)
    top += 2 + 18
    status_baseline = baseline(top, 18, 18 * NORMAL_LINE_EM)
    draw.rectangle((px(left_x), px(status_baseline - 1 - 11), px(left_x + 11) - 1, px(status_baseline - 1) - 1),
                   fill=theme.accent)
    x = left_x + 11 + 10
    x += draw_spaced(draw, x, status_baseline, status_text(version), status_face, theme.ink)
    draw_spaced(draw, x + 28, status_baseline, PLATFORM, status_face, theme.ink)

    # Right column: render 530 CSS px high, centred in a 470 px column, caption below.
    with Image.open(RENDER) as source:
        photo = source.convert("RGBA")
    photo_h_css = 530
    photo_w_css = photo_h_css * photo.width / photo.height
    photo = photo.resize((round(px(photo_w_css)), round(px(photo_h_css))), Image.Resampling.LANCZOS)
    column_x, column_w, column_top = 740, 470, 16
    canvas.paste(photo, (round(px(column_x + (column_w - photo_w_css) / 2)), round(px(column_top))), photo)
    caption_top = column_top + photo_h_css + 10
    caption_w = cap_face.getlength(CAPTION) / SCALE
    draw_spaced(draw, column_x + (column_w - caption_w) / 2, baseline(caption_top, 15, 15 * NORMAL_LINE_EM),
                CAPTION, cap_face, theme.muted)
    return canvas


def encode(image: Image.Image, text: str) -> bytes:
    """Encode lossy WebP with the generator description in EXIF."""
    exif = Image.Exif()
    exif[EXIF_IMAGE_DESCRIPTION] = text
    buffer = io.BytesIO()
    image.save(buffer, "WEBP", quality=WEBP_QUALITY, method=6, exif=exif.tobytes())
    return buffer.getvalue()


def check(output_dir: Path, version: str) -> list[str]:
    """Compare committed images with the current inputs and a fresh render."""
    errors: list[str] = []
    for theme in THEMES:
        path = output_dir / OUTPUT_NAME.format(theme=theme.name)
        if not path.is_file():
            errors.append(f"{path}: missing; run scripts/generate_readme_hero.py")
            continue
        with Image.open(path) as committed:
            text = str(committed.getexif().get(EXIF_IMAGE_DESCRIPTION, ""))
            match = DESCRIPTION_RE.match(text)
            if match is None:
                errors.append(f"{path}: generator description missing")
                continue
            if match.group("version") != version:
                errors.append(f"{path}: version mismatch: image shows {match.group('version')}, "
                              f"src/version.h is {version}")
                continue
            if match.group("digest") != input_fingerprint(version, theme.name):
                errors.append(f"{path}: stale; generator, font or render input changed")
                continue
            fresh = Image.open(io.BytesIO(encode(render(theme, version), text))).convert("RGB")
            if committed.size != fresh.size:
                errors.append(f"{path}: size {committed.size} differs from {fresh.size}")
                continue
            difference = ImageStat.Stat(ImageChops.difference(committed.convert("RGB"), fresh)).mean
            if max(difference) > MAX_MEAN_DIFFERENCE:
                errors.append(f"{path}: pixels differ from a fresh render (mean {max(difference):.2f})")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--output-dir", type=Path, default=OUTPUT_DIR)
    parser.add_argument("--check", action="store_true", help="fail if the committed images are stale")
    args = parser.parse_args()
    version = firmware_version()
    if args.check:
        errors = check(args.output_dir, version)
        for error in errors:
            print(error, file=sys.stderr)
        return 1 if errors else 0
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for theme in THEMES:
        path = args.output_dir / OUTPUT_NAME.format(theme=theme.name)
        path.write_bytes(encode(render(theme, version), description(version, theme.name)))
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
