#!/usr/bin/env python3
# Author: Axel Napolitano
# License: PolyForm-Noncommercial-1.0.0
"""Generate the numbered CLOCK front-panel illustration used by the manual.

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


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--layout", type=Path, default=DEFAULT_LAYOUT)
    p.add_argument("--artwork", type=Path, default=DEFAULT_ARTWORK)
    p.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    p.add_argument("--check", action="store_true")
    args = p.parse_args()
    svg = generate(args.layout, args.artwork)
    if args.check:
        if not args.output.exists() or args.output.read_text(encoding="utf-8") != svg:
            raise SystemExit(f"front-panel illustration is stale: {args.output}")
        return 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(svg, encoding="utf-8")
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
