#!/usr/bin/env python3
# Author: Axel Napolitano
# License: PolyForm-Noncommercial-1.0.0
"""Generate the functional VCV Rack panel from the KiCad-derived front-panel artwork.

The panel graphic is docs/assets/front-panel/clock-front-panel.svg and the control
positions are sim/panel_layout.ini; scripts/import_kicad_front_panel.py writes both
from the KiCad front-panel project. This generator only maps them to Rack's 75-DPI
panel coordinate system without inventing a second placement model or lettering.
"""
from __future__ import annotations

import argparse
import configparser
import re
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LAYOUT = ROOT / "sim" / "panel_layout.ini"
VCV_DIR = ROOT / "vcv"
DEFAULT_PANEL = VCV_DIR / "res" / "CLOCK.svg"
DEFAULT_HEADER = VCV_DIR / "generated_panel_layout.hpp"
PANEL_ARTWORK = ROOT / "docs" / "assets" / "front-panel" / "clock-front-panel.svg"
LEGACY_ENCODER_ASSETS = (
    VCV_DIR / "res" / "encoder.svg",
    VCV_DIR / "res" / "encoder-push-0.svg",
    VCV_DIR / "res" / "encoder-push-1.svg",
)

RACK_DPI = 75.0
MM_PER_INCH = 25.4
PX_PER_MM = RACK_DPI / MM_PER_INCH
RACK_WIDTH_PX = 150.0  # 10 HP
RACK_HEIGHT_PX = 380.0


@dataclass(frozen=True)
class Point:
    x: float
    y: float


def fmt(value: float) -> str:
    return f"{value:.4f}".rstrip("0").rstrip(".")


def cpp_float(value: float) -> str:
    """Format a standard C++ floating literal with an explicit decimal point."""
    text = fmt(value)
    if "." not in text and "e" not in text.lower():
        text += ".0"
    return f"{text}F"


def read_layout(path: Path) -> configparser.ConfigParser:
    parser = configparser.ConfigParser(interpolation=None)
    with path.open("r", encoding="utf-8") as stream:
        parser.read_file(stream)
    return parser


def panel_offsets(parser: configparser.ConfigParser) -> tuple[float, float]:
    panel_width_px = parser.getfloat("panel", "width_mm") * PX_PER_MM
    panel_height_px = parser.getfloat("panel", "height_mm") * PX_PER_MM
    return ((RACK_WIDTH_PX - panel_width_px) * 0.5, (RACK_HEIGHT_PX - panel_height_px) * 0.5)


def point_mm(parser: configparser.ConfigParser, section: str) -> Point:
    return Point(parser.getfloat(section, "x_mm"), parser.getfloat(section, "y_mm"))


def to_px(value_mm: float) -> float:
    return value_mm * PX_PER_MM


def point_px(parser: configparser.ConfigParser, section: str) -> Point:
    ox, oy = panel_offsets(parser)
    p = point_mm(parser, section)
    return Point(ox + to_px(p.x), oy + to_px(p.y))


def panel_artwork_elements() -> tuple[str, str]:
    """Return (panel path data, engraving elements) from the canonical KiCad-derived artwork.

    NanoSVG ignores clip paths, so the engraving is emitted unclipped; the Rack widgets for
    jacks, buttons, LEDs, encoder and display cover every hole.
    """
    text = PANEL_ARTWORK.read_text(encoding="utf-8")
    panel = re.search(r'<path id="panel" [^>]*\bd="([^"]+)"', text)
    engraving = re.search(r'<g id="engraving"[^>]*>\n(.*?)\n  </g>', text, re.S)
    if panel is None or engraving is None:
        raise RuntimeError(f"unexpected front-panel artwork structure: {PANEL_ARTWORK}")
    return panel.group(1), engraving.group(1)


def generated_header(parser: configparser.ConfigParser) -> str:
    display = (
        parser.getfloat("display", "x_mm"),
        parser.getfloat("display", "y_mm"),
        parser.getfloat("display", "width_mm"),
        parser.getfloat("display", "height_mm"),
    )
    outputs = [point_mm(parser, f"output_{i}") for i in range(1, 9)]
    leds = [
        Point(
            parser.getfloat(f"output_{i}", "led_x_mm"),
            parser.getfloat(f"output_{i}", "led_y_mm"),
        )
        for i in range(1, 9)
    ]
    screws = [point_mm(parser, f"screw_{i}") for i in range(1, 5)]

    def p(point: Point) -> str:
        return f"{{{cpp_float(point.x)}, {cpp_float(point.y)}}}"

    return f'''/**
 * @file generated_panel_layout.hpp
 * @brief Generated VCV Rack front-panel geometry sourced from sim/panel_layout.ini.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * GENERATED FILE. Run scripts/generate_vcv_panel.py after changing simulator geometry.
 */
#pragma once

#include <array>

namespace clockfw::vcv::panel {{

struct PointMm final {{
    float x;
    float y;
}};

struct RectMm final {{
    float x;
    float y;
    float width;
    float height;
}};

inline constexpr float kPanelWidthMm = {cpp_float(parser.getfloat("panel", "width_mm"))};
inline constexpr float kPanelHeightMm = {cpp_float(parser.getfloat("panel", "height_mm"))};
inline constexpr float kPanelOffsetXmm = {cpp_float((RACK_WIDTH_PX / PX_PER_MM - parser.getfloat("panel", "width_mm")) * 0.5)};
inline constexpr float kPanelOffsetYmm = {cpp_float((RACK_HEIGHT_PX / PX_PER_MM - parser.getfloat("panel", "height_mm")) * 0.5)};
inline constexpr RectMm kDisplay{{{cpp_float(display[0])}, {cpp_float(display[1])}, {cpp_float(display[2])}, {cpp_float(display[3])}}};
inline constexpr PointMm kEncoderCenter{p(point_mm(parser, "encoder"))};
inline constexpr float kEncoderDiameterMm = {cpp_float(parser.getfloat("encoder", "knob_diameter_mm"))};
inline constexpr PointMm kPlayCenter{p(point_mm(parser, "play"))};
inline constexpr PointMm kTapCenter{p(point_mm(parser, "tap"))};
inline constexpr PointMm kStopCenter{p(point_mm(parser, "stop"))};
inline constexpr float kButtonActuatorDiameterMm = {cpp_float(parser.getfloat("play", "actuator_diameter_mm"))};
inline constexpr PointMm kSyncCenter{p(point_mm(parser, "sync"))};
inline constexpr PointMm kResetCenter{p(point_mm(parser, "reset"))};
inline constexpr float kJackNutDiameterMm = {cpp_float(parser.getfloat("jack_ts", "nut_diameter_mm"))};
inline constexpr float kLedDiameterMm = {cpp_float(parser.getfloat("leds", "diameter_mm"))};
inline constexpr std::array<PointMm, 8U> kOutputCenters{{{{
    {', '.join(p(x) for x in outputs)}
}}}};
inline constexpr std::array<PointMm, 8U> kLedCenters{{{{
    {', '.join(p(x) for x in leds)}
}}}};
inline constexpr std::array<PointMm, 4U> kScrewCenters{{{{
    {', '.join(p(x) for x in screws)}
}}}};

}}  // namespace clockfw::vcv::panel
'''


def circle_svg(size_px: float, fill: str, pressed: bool = False) -> str:
    center = size_px * 0.5
    radius = max(1.0, center - 1.0)
    highlight = 0.24 if pressed else 0.48
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{fmt(size_px)}" height="{fmt(size_px)}" viewBox="0 0 {fmt(size_px)} {fmt(size_px)}">
  <circle cx="{fmt(center)}" cy="{fmt(center)}" r="{fmt(radius)}" fill="{fill}" stroke="#191a1c" stroke-width="1.4"/>
  <circle cx="{fmt(center - radius * 0.32)}" cy="{fmt(center - radius * 0.32)}" r="{fmt(max(0.8, radius * 0.10))}" fill="#ffffff" fill-opacity="{highlight}"/>
</svg>
'''


def generated_panel_svg(parser: configparser.ConfigParser) -> str:
    ox, oy = panel_offsets(parser)
    screws = [point_px(parser, f"screw_{i}") for i in range(1, 5)]
    panel_d, engraving = panel_artwork_elements()
    engraving_colour = re.search(r'<g id="engraving"[^>]* fill="(#[0-9a-fA-F]{6})"',
                                 PANEL_ARTWORK.read_text(encoding="utf-8")).group(1)
    panel_colour = re.search(r'<path id="panel" fill="(#[0-9a-fA-F]{6})"',
                             PANEL_ARTWORK.read_text(encoding="utf-8")).group(1)

    lines: list[str] = []
    a = lines.append
    a('<svg xmlns="http://www.w3.org/2000/svg" width="150" height="380" viewBox="0 0 150 380" role="img" aria-labelledby="title desc">')
    a('  <title id="title">South Signal Lab Clock functional VCV Rack panel</title>')
    a('  <desc id="desc">10 HP South Signal Lab Clock panel. Panel graphic derived from the KiCad front-panel project; control positions from sim/panel_layout.ini.</desc>')
    a('  <!-- GENERATED from docs/assets/front-panel/clock-front-panel.svg and sim/panel_layout.ini by scripts/generate_vcv_panel.py. -->')
    a('  <rect width="150" height="380" fill="#050506"/>')
    a(f'  <g transform="translate({fmt(ox)} {fmt(oy)}) scale({fmt(PX_PER_MM)})">')
    a(f'    <path fill="{panel_colour}" fill-rule="evenodd" d="{panel_d}"/>')
    a(f'    <g fill="{engraving_colour}" stroke="{engraving_colour}" stroke-linecap="round" stroke-linejoin="round">')
    a(engraving)
    a('    </g>')
    a('  </g>')
    for screw in screws:
        a(f'  <circle cx="{fmt(screw.x)}" cy="{fmt(screw.y)}" r="2.0" fill="#a8aaad" stroke="#696b6e" stroke-width="0.5"/>')
    a('</svg>')
    return "\n".join(lines) + "\n"


def outputs(parser: configparser.ConfigParser) -> dict[Path, str]:
    button_size = to_px(parser.getfloat("play", "actuator_diameter_mm"))
    return {
        DEFAULT_PANEL: generated_panel_svg(parser),
        DEFAULT_HEADER: generated_header(parser),
        VCV_DIR / "res" / "button-play-0.svg": circle_svg(button_size, parser["play"]["color"], False),
        VCV_DIR / "res" / "button-play-1.svg": circle_svg(button_size, "#7A1616", True),
        VCV_DIR / "res" / "button-tap-0.svg": circle_svg(button_size, parser["tap"]["color"], False),
        VCV_DIR / "res" / "button-tap-1.svg": circle_svg(button_size, "#4F4F4F", True),
        VCV_DIR / "res" / "button-stop-0.svg": circle_svg(button_size, parser["stop"]["color"], False),
        VCV_DIR / "res" / "button-stop-1.svg": circle_svg(button_size, "#090909", True),
    }


def main() -> int:
    argp = argparse.ArgumentParser()
    argp.add_argument("--layout", type=Path, default=DEFAULT_LAYOUT)
    argp.add_argument("--check", action="store_true")
    args = argp.parse_args()

    parser = read_layout(args.layout)
    generated = outputs(parser)
    if args.check:
        stale = [path for path, content in generated.items() if not path.exists() or path.read_text(encoding="utf-8") != content]
        obsolete = [path for path in LEGACY_ENCODER_ASSETS if path.exists()]
        if stale or obsolete:
            details = [str(path.relative_to(ROOT)) for path in stale]
            details.extend(f"obsolete:{path.relative_to(ROOT)}" for path in obsolete)
            raise SystemExit("stale generated VCV panel files: " + ", ".join(details))
        return 0

    for path in LEGACY_ENCODER_ASSETS:
        if path.exists():
            path.unlink()
            print(f"removed {path.relative_to(ROOT)}")

    for path, content in generated.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        print(path.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
