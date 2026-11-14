#!/usr/bin/env python3
# Author: Axel Napolitano
# License: PolyForm-Noncommercial-1.0.0
"""Generate the CLOCK front-panel illustrations used by the documentation.

Three files are written: the numbered illustration on the dark panel used by
the user guide and wiki (front-panel-anatomy.svg), and two black-on-white line
drawings for the printed manual, with numbered callouts
(front-panel-anatomy-print.svg, chapter 2) and without (front-panel-print.svg,
cover).

The panel itself is the KiCad-derived artwork in
docs/assets/front-panel/clock-front-panel.svg, written by
scripts/import_kicad_front_panel.py from the KiCad front-panel project. This
script only places that artwork on the page and adds numbered callouts whose
targets come from sim/panel_layout.ini, which the same importer generates.
The result is a documentation illustration, not a manufacturing drawing.
"""
from __future__ import annotations

import argparse
import configparser
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LAYOUT = ROOT / "sim" / "panel_layout.ini"
DEFAULT_ARTWORK = ROOT / "docs" / "assets" / "front-panel" / "clock-front-panel.svg"
DEFAULT_OUTPUT = ROOT / "docs" / "manual-source" / "assets" / "front-panel-anatomy.svg"
ACCENT = "#0B4FC0"  # existing CLOCK manual accent


def f(v: float) -> str:
    return f"{v:.2f}".rstrip("0").rstrip(".")


def load(path: Path):
    c = configparser.ConfigParser(interpolation=None)
    c.read(path, encoding="utf-8")
    return c


def artwork_body(artwork: Path) -> str:
    """Return the drawable content of the canonical panel SVG (defs, panel, engraving)."""
    text = artwork.read_text(encoding="utf-8")
    match = re.search(r"(  <defs>.*?</g>)\n</svg>", text, re.S)
    if match is None:
        raise SystemExit(f"unexpected front-panel artwork structure: {artwork}")
    return match.group(1)


def generate(layout_path: Path, artwork_path: Path = DEFAULT_ARTWORK) -> str:
    c = load(layout_path)
    pw = c.getfloat("panel", "width_mm")
    ph = c.getfloat("panel", "height_mm")

    # Drawing coordinate system: 4 px per physical mm.
    scale = 4.0
    panel_w = pw * scale
    panel_h = ph * scale
    panel_x = 320.0
    panel_y = 44.0
    vb_x = 235.0
    vb_y = 30.0
    vb_w = 430.0
    vb_h = 545.0

    def xy(section: str):
        return (
            panel_x + c.getfloat(section, "x_mm") * scale,
            panel_y + c.getfloat(section, "y_mm") * scale,
        )

    display_x = panel_x + c.getfloat("display", "x_mm") * scale
    display_y = panel_y + c.getfloat("display", "y_mm") * scale
    display_h = c.getfloat("display", "height_mm") * scale
    enc_x, enc_y = xy("encoder")
    knob_r = c.getfloat("encoder", "knob_diameter_mm") * scale / 2
    play = xy("play")
    tap = xy("tap")
    stop = xy("stop")
    button_r = c.getfloat("play", "actuator_diameter_mm") * scale / 2
    sync = xy("sync")
    reset = xy("reset")
    jack_r = c.getfloat("jack_ts", "bushing_diameter_mm") * scale / 2
    led_r = c.getfloat("leds", "diameter_mm") * scale / 2
    outs = []
    for i in range(1, 9):
        s = f"output_{i}"
        x, y = xy(s)
        lx = panel_x + c.getfloat(s, "led_x_mm") * scale
        ly = panel_y + c.getfloat(s, "led_y_mm") * scale
        outs.append((i, x, y, lx, ly))

    # Callout targets are real element geometry; label positions are layout-only.
    callouts = [
        (1, "left", display_x + 4, display_y + display_h * 0.52, 278, display_y + display_h * 0.52),
        (2, "right", enc_x + knob_r, enc_y, 610, enc_y),
        (3, "left", play[0] - button_r, play[1], 278, play[1]),
        (4, "right", tap[0] + button_r * 0.65, tap[1] + button_r * 0.65, 610, tap[1] + 30),
        (5, "right", stop[0] + button_r, stop[1], 610, stop[1]),
        (6, "left", sync[0] - jack_r, sync[1], 278, sync[1]),
        (7, "right", reset[0] + jack_r, reset[1], 610, reset[1]),
        (8, "left", outs[4][1] - jack_r, outs[4][2], 278, outs[4][2]),
        (9, "right", outs[3][3] + led_r, outs[3][4], 610, outs[3][4]),
    ]

    s = []
    a = s.append
    a(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{f(vb_x)} {f(vb_y)} {f(vb_w)} {f(vb_h)}" '
      'role="img" aria-labelledby="anatomy-title anatomy-desc">')
    a(f'  <rect x="{f(vb_x)}" y="{f(vb_y)}" width="{f(vb_w)}" height="{f(vb_h)}" fill="#fff"/>')
    a('  <title id="anatomy-title">CLOCK front-panel controls</title>')
    a('  <desc id="anatomy-desc">Numbered illustration of the CLOCK Eurorack front panel. The panel artwork is '
      'derived from the KiCad front-panel project; callout targets come from the generated panel layout.</desc>')
    a('  <!-- Generated from docs/assets/front-panel/clock-front-panel.svg and sim/panel_layout.ini; '
      'documentation illustration, not a manufacturing drawing. -->')
    a('  <style>.accentLine{stroke:' + ACCENT + ';stroke-width:1.8;fill:none}.accentDisc{fill:' + ACCENT + '}'
      '.num{font-family:Ubuntu,Arial,sans-serif;font-size:11px;font-weight:700;fill:white;'
      'text-anchor:middle;dominant-baseline:middle}</style>')
    a(f'  <g transform="translate({f(panel_x)} {f(panel_y)}) scale({f(scale)})">')
    a(artwork_body(artwork_path))
    a('  </g>')
    a(f'  <rect x="{f(panel_x)}" y="{f(panel_y)}" width="{f(panel_w)}" height="{f(panel_h)}" '
      'fill="none" stroke="#202020" stroke-width="1.2"/>')

    # Callout leaders and numbered discs. Line terminates at disc edge rather than through it.
    disc_r = 12.5
    for n, side, tx, ty, cx, cy in callouts:
        line_end = cx + disc_r if side == "left" else cx - disc_r
        a(f'  <path class="accentLine" d="M {f(tx)} {f(ty)} L {f(line_end)} {f(cy)}"/>')
        a(f'  <circle class="accentDisc" cx="{f(cx)}" cy="{f(cy)}" r="{f(disc_r)}"/>')
        a(f'  <text class="num" x="{f(cx)}" y="{f(cy + .5)}">{n}</text>')

    a('</svg>')
    return "\n".join(s) + "\n"


# Print line drawing (D-12 points 4 and 10): black on white, no panel fill.
PRINT_INK = "#000"
PRINT_PANEL_STROKE_MM = 0.18
PRINT_LEADER_STROKE_MM = 0.15
PRINT_LEADER_DOT_RADIUS_MM = 0.45
PRINT_CALLOUT_RADIUS_MM = 2.2
PRINT_CALLOUT_STROKE_MM = 0.18
PRINT_CALLOUT_FONT_MM = 2.7
PRINT_CALLOUT_LEFT_X_MM = -9.0
PRINT_CALLOUT_RIGHT_X_MM = 59.5
PRINT_ELBOW_RUN_MM = 2.0
PRINT_CALLOUTS_ON_LEFT = (1, 3, 6, 8)
#: Printed width of the illustration with callouts (D-12 point 10).
PRINT_CALLOUT_WIDTH_MM = 66.0
DEFAULT_PRINT_OUTPUT = ROOT / "docs" / "manual-source" / "assets" / "front-panel-print.svg"
DEFAULT_PRINT_CALLOUT_OUTPUT = (
    ROOT / "docs" / "manual-source" / "assets" / "front-panel-anatomy-print.svg"
)


def line_artwork_body(artwork: Path) -> str:
    """Return the panel artwork as a black-on-white line drawing.

    The panel outline and cut-outs become a hairline without fill; the
    engraving keeps its geometry and turns black, so filled number fields stay
    filled and their digits stay open, as engraved.
    """
    body = artwork_body(artwork)
    panel = re.search(r'<path id="panel" fill="[^"]*"', body)
    engraving = re.search(r'<g id="engraving" clip-path="url\(#panel-clip\)" fill="[^"]*" stroke="[^"]*"', body)
    if panel is None or engraving is None:
        raise SystemExit(f"unexpected front-panel artwork structure: {artwork}")
    body = body.replace(
        panel.group(0),
        f'<path id="panel" fill="none" stroke="{PRINT_INK}" stroke-width="{f(PRINT_PANEL_STROKE_MM)}"',
        1,
    )
    return body.replace(
        engraving.group(0),
        f'<g id="engraving" clip-path="url(#panel-clip)" fill="{PRINT_INK}" stroke="{PRINT_INK}"',
        1,
    )


def print_callouts(c: configparser.ConfigParser) -> list[tuple[int, float, float, float]]:
    """Return (number, target x, target y, label y) for every panel callout, in mm."""

    def at(section: str) -> tuple[float, float]:
        return c.getfloat(section, "x_mm"), c.getfloat(section, "y_mm")

    display_x, display_y = at("display")
    display_w = c.getfloat("display", "width_mm")
    display_h = c.getfloat("display", "height_mm")
    enc_x, enc_y = at("encoder")
    knob_r = c.getfloat("encoder", "knob_diameter_mm") / 2
    button_r = c.getfloat("play", "actuator_diameter_mm") / 2
    jack_r = c.getfloat("jack_ts", "bushing_diameter_mm") / 2
    led_r = c.getfloat("leds", "diameter_mm") / 2
    play_x, play_y = at("play")
    tap_x, tap_y = at("tap")
    stop_x, stop_y = at("stop")
    sync_x, sync_y = at("sync")
    reset_x, reset_y = at("reset")
    out1_x, out1_y = at("output_1")
    led4_x = c.getfloat("output_4", "led_x_mm")
    led4_y = c.getfloat("output_4", "led_y_mm")
    display_mid_y = display_y + display_h / 2
    # Label rows above their target keep the encoder and TAP callouts clear of
    # the neighbouring STOP / display callouts.
    return [
        (1, display_x + display_w / 2, display_mid_y, display_mid_y),
        (2, enc_x + knob_r, enc_y, display_y - 0.25),
        (3, play_x - button_r, play_y, play_y),
        (4, tap_x, tap_y - button_r, tap_y - button_r - 3.75),
        (5, stop_x + button_r, stop_y, stop_y),
        (6, sync_x - jack_r, sync_y, sync_y),
        (7, reset_x + jack_r, reset_y, reset_y),
        (8, out1_x - jack_r, out1_y, out1_y),
        (9, led4_x + led_r, led4_y, led4_y),
    ]


def generate_print(layout_path: Path, artwork_path: Path = DEFAULT_ARTWORK, *, callouts: bool) -> str:
    """Return the print line drawing, with or without numbered callouts."""
    c = load(layout_path)
    pw = c.getfloat("panel", "width_mm")
    ph = c.getfloat("panel", "height_mm")
    margin = PRINT_PANEL_STROKE_MM
    if callouts:
        left = PRINT_CALLOUT_LEFT_X_MM - PRINT_CALLOUT_RADIUS_MM - margin
        right = PRINT_CALLOUT_RIGHT_X_MM + PRINT_CALLOUT_RADIUS_MM + margin
    else:
        left, right = -margin, pw + margin
    top, bottom = -margin, ph + margin
    width, height = right - left, bottom - top
    title = "CLOCK front panel with numbered controls" if callouts else "CLOCK front panel"
    s = []
    a = s.append
    a(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{f(left)} {f(top)} {f(width)} {f(height)}" '
      f'width="{f(width)}mm" height="{f(height)}mm" role="img" aria-labelledby="title desc">')
    a(f'  <title id="title">{title}</title>')
    a('  <desc id="desc">Black-on-white line drawing of the CLOCK Eurorack front panel for the printed '
      'manual. The panel artwork is derived from the KiCad front-panel project'
      + ('; callout targets come from the generated panel layout.' if callouts else '.') + '</desc>')
    a('  <!-- Generated from docs/assets/front-panel/clock-front-panel.svg and sim/panel_layout.ini; '
      'documentation illustration, not a manufacturing drawing. -->')
    a(line_artwork_body(artwork_path))
    if callouts:
        for number, tx, ty, label_y in print_callouts(c):
            on_left = number in PRINT_CALLOUTS_ON_LEFT
            cx = PRINT_CALLOUT_LEFT_X_MM if on_left else PRINT_CALLOUT_RIGHT_X_MM
            edge = cx + PRINT_CALLOUT_RADIUS_MM if on_left else cx - PRINT_CALLOUT_RADIUS_MM
            if abs(label_y - ty) < 1e-9:
                points = [(tx, ty), (edge, ty)]
            else:
                elbow_x = tx - PRINT_ELBOW_RUN_MM if on_left else tx + PRINT_ELBOW_RUN_MM
                points = [(tx, ty), (elbow_x, label_y), (edge, label_y)]
            a('  <polyline points="' + " ".join(f"{f(x)},{f(y)}" for x, y in points)
              + f'" fill="none" stroke="{PRINT_INK}" stroke-width="{f(PRINT_LEADER_STROKE_MM)}"/>')
            a(f'  <circle cx="{f(tx)}" cy="{f(ty)}" r="{f(PRINT_LEADER_DOT_RADIUS_MM)}" fill="{PRINT_INK}"/>')
            a(f'  <circle cx="{f(cx)}" cy="{f(label_y)}" r="{f(PRINT_CALLOUT_RADIUS_MM)}" fill="#fff" '
              f'stroke="{PRINT_INK}" stroke-width="{f(PRINT_CALLOUT_STROKE_MM)}"/>')
            a(f'  <text x="{f(cx)}" y="{f(label_y + 0.95)}" text-anchor="middle" font-family="Ubuntu" '
              f'font-weight="700" font-size="{f(PRINT_CALLOUT_FONT_MM)}" fill="{PRINT_INK}">{number}</text>')
    a('</svg>')
    return "\n".join(s) + "\n"


def write_or_check(path: Path, svg: str, check: bool) -> None:
    if check:
        if not path.exists() or path.read_text(encoding="utf-8") != svg:
            raise SystemExit(f"front-panel illustration is stale: {path}")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(svg, encoding="utf-8", newline="\n")
    print(path)


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--layout", type=Path, default=DEFAULT_LAYOUT)
    p.add_argument("--artwork", type=Path, default=DEFAULT_ARTWORK)
    p.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    p.add_argument("--print-output", type=Path, default=DEFAULT_PRINT_OUTPUT)
    p.add_argument("--print-callout-output", type=Path, default=DEFAULT_PRINT_CALLOUT_OUTPUT)
    p.add_argument("--check", action="store_true")
    args = p.parse_args()
    write_or_check(args.output, generate(args.layout, args.artwork), args.check)
    write_or_check(
        args.print_output, generate_print(args.layout, args.artwork, callouts=False), args.check
    )
    write_or_check(
        args.print_callout_output,
        generate_print(args.layout, args.artwork, callouts=True),
        args.check,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
