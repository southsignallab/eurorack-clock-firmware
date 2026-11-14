#!/usr/bin/env python3
"""Generate the black-on-white QR codes printed in the CLOCK user manual.

The manual carries three QR codes: firmware updates (cover), source repository
and Ko-fi support (Licenses and source). They are printed at home, so each code
is plain black on white without a logo or colour. The encoder below is a small
self-contained implementation of ISO/IEC 18004 byte mode with error-correction
level M, so the build needs no QR package.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import argparse
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:  # pragma: no cover - exercised by CLI diagnostics
    raise SystemExit("Pillow is required: python -m pip install pillow") from exc

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "docs" / "manual-source" / "assets"

UPDATES_URL = "https://github.com/southsignallab/eurorack-clock-firmware/releases"
REPOSITORY_URL = "https://github.com/southsignallab/eurorack-clock-firmware"
KOFI_URL = "https://ko-fi.com/southsignallab"

#: Published QR codes: asset file name -> encoded URL.
QR_CODES: dict[str, str] = {
    "updates-qr.png": UPDATES_URL,
    "repository-qr.png": REPOSITORY_URL,
    "kofi-support-qr.png": KOFI_URL,
}

MODULE_PX = 10  # pixels per QR module in the PNG
QUIET_ZONE_MODULES = 4  # standard quiet zone around the symbol

# Error-correction level M, versions 1..10: (EC codewords per block, number of blocks).
_EC_M = {
    1: (10, 1), 2: (16, 1), 3: (26, 1), 4: (18, 2), 5: (24, 2),
    6: (16, 4), 7: (18, 4), 8: (22, 4), 9: (22, 5), 10: (26, 5),
}
_EC_LEVEL_M_FORMAT_BITS = 0b00


def _raw_data_modules(version: int) -> int:
    """Return the number of data and EC modules of one symbol version."""
    result = (16 * version + 128) * version + 64
    if version >= 2:
        alignment_count = version // 7 + 2
        result -= (25 * alignment_count - 10) * alignment_count - 55
        if version >= 7:
            result -= 36
    return result


def _data_codewords(version: int) -> int:
    ec_len, blocks = _EC_M[version]
    return _raw_data_modules(version) // 8 - ec_len * blocks


def _alignment_positions(version: int) -> list[int]:
    if version == 1:
        return []
    count = version // 7 + 2
    size = version * 4 + 17
    step = (version * 8 + count * 3 + 5) // (count * 4 - 4) * 2
    positions = [size - 7 - i * step for i in range(count - 1)]
    return [6] + sorted(positions)


def _gf_multiply(x: int, y: int) -> int:
    """Multiply in GF(2^8) modulo x^8 + x^4 + x^3 + x^2 + 1."""
    z = 0
    for i in range(7, -1, -1):
        z = (z << 1) ^ ((z >> 7) * 0x11D)
        z ^= ((y >> i) & 1) * x
    return z & 0xFF


def _rs_divisor(degree: int) -> list[int]:
    result = [0] * (degree - 1) + [1]
    root = 1
    for _ in range(degree):
        for j in range(degree):
            result[j] = _gf_multiply(result[j], root)
            if j + 1 < degree:
                result[j] ^= result[j + 1]
        root = _gf_multiply(root, 0x02)
    return result


def _rs_remainder(data: list[int], divisor: list[int]) -> list[int]:
    result = [0] * len(divisor)
    for byte in data:
        factor = byte ^ result.pop(0)
        result.append(0)
        for i, coefficient in enumerate(divisor):
            result[i] ^= _gf_multiply(coefficient, factor)
    return result


def _encode_codewords(payload: bytes) -> tuple[int, list[int]]:
    """Return the smallest version and its interleaved data+EC codewords."""
    for version in range(1, 11):
        count_bits = 8 if version < 10 else 16
        capacity_bits = _data_codewords(version) * 8
        if 4 + count_bits + 8 * len(payload) <= capacity_bits:
            break
    else:
        raise ValueError(f"Payload too long for a version-10 QR code: {len(payload)} bytes")

    bits: list[int] = []

    def append(value: int, length: int) -> None:
        bits.extend((value >> i) & 1 for i in range(length - 1, -1, -1))

    append(0b0100, 4)  # byte mode
    append(len(payload), count_bits)
    for byte in payload:
        append(byte, 8)
    append(0, min(4, capacity_bits - len(bits)))
    append(0, -len(bits) % 8)
    pad = 0xEC
    while len(bits) < capacity_bits:
        append(pad, 8)
        pad ^= 0xEC ^ 0x11
    data = [int("".join(map(str, bits[i:i + 8])), 2) for i in range(0, len(bits), 8)]

    ec_len, block_count = _EC_M[version]
    raw_codewords = _raw_data_modules(version) // 8
    short_blocks = block_count - raw_codewords % block_count
    short_len = raw_codewords // block_count
    divisor = _rs_divisor(ec_len)
    blocks: list[list[int]] = []
    offset = 0
    for index in range(block_count):
        length = short_len - ec_len + (0 if index < short_blocks else 1)
        block = data[offset:offset + length]
        offset += length
        ecc = _rs_remainder(block, divisor)
        if index < short_blocks:
            block = block + [None]  # placeholder keeps data columns aligned
        blocks.append(block + ecc)
    result = []
    for column in range(len(blocks[0])):
        for index, block in enumerate(blocks):
            if column != short_len - ec_len or index >= short_blocks:
                result.append(block[column])
    return version, result


class _Symbol:
    def __init__(self, version: int) -> None:
        self.version = version
        self.size = version * 4 + 17
        self.modules = [[False] * self.size for _ in range(self.size)]
        self.function = [[False] * self.size for _ in range(self.size)]

    def set_function(self, x: int, y: int, dark: bool) -> None:
        self.modules[y][x] = dark
        self.function[y][x] = True

    def draw_function_patterns(self) -> None:
        size = self.size
        for i in range(size):
            self.set_function(6, i, i % 2 == 0)
            self.set_function(i, 6, i % 2 == 0)
        for cx, cy in ((3, 3), (size - 4, 3), (3, size - 4)):
            for dy in range(-4, 5):
                for dx in range(-4, 5):
                    x, y = cx + dx, cy + dy
                    if 0 <= x < size and 0 <= y < size:
                        distance = max(abs(dx), abs(dy))
                        self.set_function(x, y, distance not in (2, 4))
        positions = _alignment_positions(self.version)
        last = len(positions) - 1
        for i, cx in enumerate(positions):
            for j, cy in enumerate(positions):
                if (i, j) in ((0, 0), (0, last), (last, 0)):
                    continue
                for dy in range(-2, 3):
                    for dx in range(-2, 3):
                        self.set_function(cx + dx, cy + dy, max(abs(dx), abs(dy)) != 1)
        self.draw_format_bits(0)
        self.draw_version()

    def draw_format_bits(self, mask: int) -> None:
        data = _EC_LEVEL_M_FORMAT_BITS << 3 | mask
        remainder = data
        for _ in range(10):
            remainder = (remainder << 1) ^ ((remainder >> 9) * 0x537)
        bits = (data << 10 | remainder) ^ 0x5412
        size = self.size

        def bit(i: int) -> bool:
            return (bits >> i) & 1 != 0

        for i in range(6):
            self.set_function(8, i, bit(i))
        self.set_function(8, 7, bit(6))
        self.set_function(8, 8, bit(7))
        self.set_function(7, 8, bit(8))
        for i in range(9, 15):
            self.set_function(14 - i, 8, bit(i))
        for i in range(8):
            self.set_function(size - 1 - i, 8, bit(i))
        for i in range(8, 15):
            self.set_function(8, size - 15 + i, bit(i))
        self.set_function(8, size - 8, True)

    def draw_version(self) -> None:
        if self.version < 7:
            return
        remainder = self.version
        for _ in range(12):
            remainder = (remainder << 1) ^ ((remainder >> 11) * 0x1F25)
        bits = self.version << 12 | remainder
        for i in range(18):
            dark = (bits >> i) & 1 != 0
            a, b = self.size - 11 + i % 3, i // 3
            self.set_function(a, b, dark)
            self.set_function(b, a, dark)

    def draw_codewords(self, codewords: list[int]) -> None:
        i = 0
        total = len(codewords) * 8
        right = self.size - 1
        while right >= 1:
            if right == 6:
                right = 5
            for vertical in range(self.size):
                for j in range(2):
                    x = right - j
                    upward = (right + 1) & 2 == 0
                    y = self.size - 1 - vertical if upward else vertical
                    if not self.function[y][x] and i < total:
                        self.modules[y][x] = (codewords[i >> 3] >> (7 - (i & 7))) & 1 != 0
                        i += 1
            right -= 2

    def apply_mask(self, mask: int) -> None:
        for y in range(self.size):
            for x in range(self.size):
                if self.function[y][x]:
                    continue
                invert = (
                    (x + y) % 2 == 0,
                    y % 2 == 0,
                    x % 3 == 0,
                    (x + y) % 3 == 0,
                    (x // 3 + y // 2) % 2 == 0,
                    x * y % 2 + x * y % 3 == 0,
                    (x * y % 2 + x * y % 3) % 2 == 0,
                    ((x + y) % 2 + x * y % 3) % 2 == 0,
                )[mask]
                if invert:
                    self.modules[y][x] = not self.modules[y][x]

    def penalty(self) -> int:
        size = self.size
        result = 0
        lines = [row[:] for row in self.modules] + [
            [self.modules[y][x] for y in range(size)] for x in range(size)
        ]
        finder_like = ([True, False, True, True, True, False, True, False, False, False, False],
                       [False, False, False, False, True, False, True, True, True, False, True])
        for line in lines:
            run_colour, run_length = line[0], 1
            for value in line[1:]:
                if value == run_colour:
                    run_length += 1
                else:
                    if run_length >= 5:
                        result += 3 + run_length - 5
                    run_colour, run_length = value, 1
            if run_length >= 5:
                result += 3 + run_length - 5
            padded = [False] * 4 + line + [False] * 4
            for start in range(len(padded) - 10):
                if padded[start:start + 11] in finder_like:
                    result += 40
        for y in range(size - 1):
            for x in range(size - 1):
                colour = self.modules[y][x]
                if (colour == self.modules[y][x + 1] == self.modules[y + 1][x]
                        == self.modules[y + 1][x + 1]):
                    result += 3
        dark = sum(sum(row) for row in self.modules)
        total = size * size
        k = (abs(dark * 20 - total * 10) + total - 1) // total - 1
        result += k * 10
        return result


def encode(text: str, mask: int | None = None) -> list[list[bool]]:
    """Return the module matrix (row-major, True = dark) for *text*.

    Without *mask* the mask with the lowest penalty score is chosen.
    """
    version, codewords = _encode_codewords(text.encode("utf-8"))
    best: tuple[int, list[list[bool]]] | None = None
    for candidate in range(8) if mask is None else (mask,):
        symbol = _Symbol(version)
        symbol.draw_function_patterns()
        symbol.draw_codewords(codewords)
        symbol.apply_mask(candidate)
        symbol.draw_format_bits(candidate)
        score = symbol.penalty()
        if best is None or score < best[0]:
            best = (score, [row[:] for row in symbol.modules])
    assert best is not None
    return best[1]


def render(matrix: list[list[bool]], module_px: int = MODULE_PX,
           quiet_zone: int = QUIET_ZONE_MODULES) -> Image.Image:
    """Render a module matrix as a 1-bit black-on-white image."""
    size = (len(matrix) + 2 * quiet_zone) * module_px
    image = Image.new("1", (size, size), 1)
    pixels = image.load()
    for y, row in enumerate(matrix):
        for x, dark in enumerate(row):
            if not dark:
                continue
            left = (x + quiet_zone) * module_px
            top = (y + quiet_zone) * module_px
            for py in range(top, top + module_px):
                for px in range(left, left + module_px):
                    pixels[px, py] = 0
    return image


def matrix_from_image(image: Image.Image) -> list[list[bool]]:
    """Read the module matrix back from a rendered, axis-aligned QR image.

    The module size is taken from the top-left finder pattern (seven modules
    wide); every module is sampled at its centre.
    """
    grey = image.convert("L")
    width, height = grey.size
    pixels = grey.load()
    dark = [(x, y) for y in range(height) for x in range(width) if pixels[x, y] < 128]
    if not dark:
        raise ValueError("QR image contains no dark modules")
    x0 = min(x for x, _ in dark)
    y0 = min(y for _, y in dark)
    x1 = max(x for x, _ in dark)
    run = 0
    while x0 + run < width and pixels[x0 + run, y0] < 128:
        run += 1
    module = run / 7
    count = round((x1 - x0 + 1) / module)
    return [
        [pixels[int(x0 + (i + 0.5) * module), int(y0 + (j + 0.5) * module)] < 128 for i in range(count)]
        for j in range(count)
    ]


def encodes(image: Image.Image, text: str) -> bool:
    """Return whether *image* shows the QR code this module generates for *text*."""
    try:
        return matrix_from_image(image) == encode(text)
    except (ValueError, IndexError):
        return False


def generate(output_dir: Path, *, check: bool) -> list[Path]:
    """Write (or with *check* compare) every manual QR code."""
    stale: list[Path] = []
    written: list[Path] = []
    for name, url in QR_CODES.items():
        path = output_dir / name
        image = render(encode(url))
        if check:
            if not path.is_file():
                stale.append(path)
                continue
            with Image.open(path) as existing:
                colours = {colour for _count, colour in existing.convert("RGB").getcolors(1 << 16) or []}
                if (existing.size != image.size
                        or not colours <= {(0, 0, 0), (255, 255, 255)}
                        or existing.convert("L").tobytes() != image.convert("L").tobytes()):
                    stale.append(path)
            continue
        output_dir.mkdir(parents=True, exist_ok=True)
        image.save(path, format="PNG", optimize=True)
        written.append(path)
    if stale:
        raise SystemExit("Manual QR codes are stale: " + ", ".join(str(path) for path in stale))
    return written


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ASSETS)
    parser.add_argument("--check", action="store_true", help="fail if a QR asset differs")
    args = parser.parse_args()
    for path in generate(args.output.resolve(), check=args.check):
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
