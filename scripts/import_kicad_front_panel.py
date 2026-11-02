#!/usr/bin/env python3
# Author: Axel Napolitano
# License: PolyForm-Noncommercial-1.0.0
"""Derive every front-panel image and coordinate from the KiCad front-panel project.

The KiCad front panel (``Simple Frontpanel.kicad_pcb`` in the CLOCK hardware
project) is the single source of truth for the panel. This importer reads it
directly, without a KiCad installation:

* ``Edge.Cuts``  -> panel outline, round holes, display window, mounting slots;
* ``F.SilkS``    -> the complete engraving (drawn geometry, no KiCad text).

It writes:

* ``docs/assets/front-panel/clock-front-panel.svg`` - canonical vector artwork
  in millimetres (holes are real cut-outs);
* ``docs/assets/front-panel/clock-front-panel.png`` - raster for web/print use,
  transparent holes;
* ``sim/panel_background.png`` - simulator panel background at 14 px/mm;
* the geometry keys of ``sim/panel_layout.ini`` (display window, encoder,
  buttons, inputs, outputs, LEDs, mounting slots, panel size).

Everything else (manual illustration, VCV Rack panel) is generated from those
committed files by ``generate_front_panel_illustration.py`` and
``generate_vcv_panel.py``, so CI can check consistency without access to the
private hardware repository. Run ``--check`` with the KiCad path to prove that
the committed files still match the KiCad project.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARTWORK_DIR = ROOT / "docs" / "assets" / "front-panel"
ARTWORK_SVG = ARTWORK_DIR / "clock-front-panel.svg"
ARTWORK_PNG = ARTWORK_DIR / "clock-front-panel.png"
SIM_BACKGROUND = ROOT / "sim" / "panel_background.png"
LAYOUT_INI = ROOT / "sim" / "panel_layout.ini"

PANEL_COLOR = "#141414"      # black matte anodized aluminium
ENGRAVING_COLOR = "#d9d9d9"  # laser engraving
SIM_HOLE_COLOR = (5, 5, 6)
PNG_PX_PER_MM = 20
SIM_PX_PER_MM = 14
SUPERSAMPLE = 4
ARC_SEGMENT_MM = 0.05
SEAM_STROKE_MM = 0.04


# --------------------------------------------------------------------------------------------
# Minimal s-expression access for the subset of .kicad_pcb used by the front panel.
# --------------------------------------------------------------------------------------------

def _children(text: str, start: int, end: int):
    """Yield (start, end) spans of the direct child lists inside text[start:end]."""
    i = start
    while True:
        j = text.find("(", i, end)
        if j < 0:
            return
        depth = 0
        k = j
        while k < end:
            ch = text[k]
            if ch == '"':
                k += 1
                while text[k] != '"':
                    if text[k] == "\\":
                        k += 1
                    k += 1
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        yield j, k + 1
        i = k + 1


def _head(item: str) -> str:
    return re.match(r"\(([\w.]+)", item).group(1)


def _layer(item: str) -> str | None:
    m = re.search(r'\(layer "([^"]+)"\)', item)
    return m.group(1) if m else None


def _point(item: str, key: str) -> tuple[float, float] | None:
    m = re.search(r"\(" + key + r" (-?[\d.]+) (-?[\d.]+)\)", item)
    return (float(m.group(1)), float(m.group(2))) if m else None


def _width(item: str) -> float:
    m = re.search(r"\(width (-?[\d.]+)\)", item)
    return float(m.group(1)) if m else 0.0


def _filled(item: str) -> bool:
    m = re.search(r"\(fill (yes|solid|no|none)\)", item)
    return bool(m and m.group(1) in ("yes", "solid"))


def _arc_center(p1, p2, p3):
    ax, ay = p1
    bx, by = p2
    cx, cy = p3
    d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by))
    ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d
    uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d
    return ux, uy


def _arc_points(p1, p2, p3) -> list[tuple[float, float]]:
    """Sample a three-point arc (start, mid, end) into a polyline including both ends."""
    cx, cy = _arc_center(p1, p2, p3)
    r = math.hypot(p1[0] - cx, p1[1] - cy)
    a1 = math.atan2(p1[1] - cy, p1[0] - cx)
    a2 = math.atan2(p2[1] - cy, p2[0] - cx)
    a3 = math.atan2(p3[1] - cy, p3[0] - cx)
    ccw = ((p2[0] - p1[0]) * (p3[1] - p1[1]) - (p2[1] - p1[1]) * (p3[0] - p1[0])) > 0

    def norm(a, ref, positive):
        while positive and a < ref:
            a += 2 * math.pi
        while not positive and a > ref:
            a -= 2 * math.pi
        return a

    a3n = norm(a3, a1, ccw)
    sweep = a3n - a1
    steps = max(4, int(abs(sweep) * r / ARC_SEGMENT_MM))
    return [(cx + r * math.cos(a1 + sweep * t / steps), cy + r * math.sin(a1 + sweep * t / steps)) for t in range(steps + 1)]


@dataclass
class Segment:
    kind: str                       # "line" or "arc"
    points: list                    # start, (mid,) end in board coordinates
    raw: tuple = ()


@dataclass
class Panel:
    source: Path
    sha256: str
    hw_rev: str
    origin: tuple[float, float] = (0.0, 0.0)
    width: float = 0.0
    height: float = 0.0
    circles: list = field(default_factory=list)        # (cx, cy, d) panel mm
    contours: list = field(default_factory=list)       # list of (segments) panel mm, closed
    engraving: list = field(default_factory=list)      # shapes in panel mm


def _chain(segments: list[Segment], tol: float = 1e-3) -> list[list[Segment]]:
    """Group Edge.Cuts lines/arcs into closed contours by matching endpoints."""
    remaining = segments[:]
    contours: list[list[Segment]] = []
    close = lambda a, b: abs(a[0] - b[0]) < tol and abs(a[1] - b[1]) < tol
    while remaining:
        contour = [remaining.pop(0)]
        start = contour[0].points[0]
        end = contour[0].points[-1]
        progressed = True
        while not close(start, end) and progressed:
            progressed = False
            for idx, seg in enumerate(remaining):
                if close(seg.points[0], end):
                    contour.append(remaining.pop(idx))
                    end = seg.points[-1]
                    progressed = True
                    break
                if close(seg.points[-1], end):
                    seg = Segment(seg.kind, list(reversed(seg.points)))
                    remaining.pop(idx)
                    contour.append(seg)
                    end = seg.points[-1]
                    progressed = True
                    break
        if not close(start, end):
            raise SystemExit("Edge.Cuts contains an open contour; fix the KiCad outline first")
        contours.append(contour)
    return contours


def load_panel(pcb_path: Path) -> Panel:
    raw = pcb_path.read_bytes()
    text = raw.decode("utf-8")
    pro_path = pcb_path.with_suffix(".kicad_pro")
    hw_rev = "?"
    if pro_path.is_file():
        try:
            hw_rev = json.loads(pro_path.read_text(encoding="utf-8"))["text_variables"].get("HW_REV", "?")
        except (KeyError, ValueError):
            hw_rev = "?"
    panel = Panel(source=pcb_path, sha256=hashlib.sha256(raw).hexdigest(), hw_rev=hw_rev)

    edge_segments: list[Segment] = []
    edge_circles = []
    silk = []
    root_start = text.find("(")
    root_end = text.rfind(")")
    for s, e in _children(text, root_start + 1, root_end):
        item = text[s:e]
        head = _head(item)
        if head == "footprint" and '"F.SilkS"' in item:
            raise SystemExit("front-panel engraving inside footprints is not supported; keep it as board graphics")
        if not head.startswith("gr_"):
            continue
        layer = _layer(item)
        if layer == "F.SilkS" and head == "gr_text":
            raise SystemExit("F.SilkS contains KiCad text; the engraving must be drawn geometry")
        if layer not in ("Edge.Cuts", "F.SilkS"):
            continue
        if head == "gr_circle":
            c, p = _point(item, "center"), _point(item, "end")
            r = math.hypot(p[0] - c[0], p[1] - c[1])
            if layer == "Edge.Cuts":
                edge_circles.append((c[0], c[1], 2 * r))
            else:
                silk.append(("circle", c, r, _width(item), _filled(item)))
        elif head == "gr_line":
            a, b = _point(item, "start"), _point(item, "end")
            if layer == "Edge.Cuts":
                edge_segments.append(Segment("line", [a, b]))
            else:
                silk.append(("line", [a, b], _width(item)))
        elif head == "gr_arc":
            pts = [_point(item, "start"), _point(item, "mid"), _point(item, "end")]
            if layer == "Edge.Cuts":
                edge_segments.append(Segment("arc", pts))
            else:
                silk.append(("polyline", _arc_points(*pts), _width(item)))
        elif head == "gr_rect":
            a, b = _point(item, "start"), _point(item, "end")
            corners = [a, (b[0], a[1]), b, (a[0], b[1])]
            if layer == "Edge.Cuts":
                for i in range(4):
                    edge_segments.append(Segment("line", [corners[i], corners[(i + 1) % 4]]))
            else:
                silk.append(("poly", corners, _width(item), _filled(item)))
        elif head == "gr_poly":
            pts = [(float(x), float(y)) for x, y in re.findall(r"\(xy (-?[\d.]+) (-?[\d.]+)\)", item)]
            if layer == "Edge.Cuts":
                for i in range(len(pts)):
                    edge_segments.append(Segment("line", [pts[i], pts[(i + 1) % len(pts)]]))
            else:
                silk.append(("poly", pts, _width(item), _filled(item) or "(fill" not in item))
        else:
            raise SystemExit(f"unsupported {head} on {layer}")

    contours = _chain(edge_segments)

    def bbox(contour):
        xs = [p[0] for seg in contour for p in seg.points]
        ys = [p[1] for seg in contour for p in seg.points]
        return min(xs), min(ys), max(xs), max(ys)

    outer = max(contours, key=lambda c: (bbox(c)[2] - bbox(c)[0]) * (bbox(c)[3] - bbox(c)[1]))
    x0, y0, x1, y1 = bbox(outer)
    panel.origin = (x0, y0)
    panel.width = round(x1 - x0, 4)
    panel.height = round(y1 - y0, 4)

    def local(p):
        return (round(p[0] - x0, 4), round(p[1] - y0, 4))

    panel.contours = [[Segment(s.kind, [local(p) for p in s.points]) for s in c] for c in contours]
    panel.circles = [(local((cx, cy))[0], local((cx, cy))[1], round(d, 4)) for cx, cy, d in edge_circles]
    for shape in silk:
        if shape[0] == "circle":
            panel.engraving.append(("circle", local(shape[1]), shape[2], shape[3], shape[4]))
        elif shape[0] in ("line", "polyline"):
            panel.engraving.append(("polyline", [local(p) for p in shape[1]], shape[2]))
        else:
            panel.engraving.append(("poly", [local(p) for p in shape[1]], shape[2], shape[3]))
    return panel


# --------------------------------------------------------------------------------------------
# Element classification (Edge.Cuts -> controls).
# --------------------------------------------------------------------------------------------

def classify(panel: Panel) -> dict:
    by_diameter: dict[float, list] = {}
    for cx, cy, d in panel.circles:
        by_diameter.setdefault(round(d, 2), []).append((cx, cy))
    sizes = sorted(by_diameter, reverse=True)
    counts = [len(by_diameter[d]) for d in sizes]
    if counts != [3, 1, 10, 8]:
        raise SystemExit(
            "unexpected round Edge.Cuts holes; expected 3 buttons, 1 encoder, 10 jacks, 8 LEDs "
            f"(largest to smallest), found {list(zip(sizes, counts))}")
    buttons = sorted(by_diameter[sizes[0]])
    encoder = by_diameter[sizes[1]][0]
    jacks = sorted(by_diameter[sizes[2]], key=lambda p: (round(p[1], 1), p[0]))
    leds = sorted(by_diameter[sizes[3]], key=lambda p: (round(p[1], 1), p[0]))
    inputs, outputs = jacks[:2], jacks[2:]
    if not all(i[1] < o[1] for i in inputs for o in outputs):
        raise SystemExit("inputs must form the jack row above the outputs")

    inner = []
    for contour in panel.contours:
        xs = [p[0] for seg in contour for p in seg.points]
        ys = [p[1] for seg in contour for p in seg.points]
        bx = (min(xs), min(ys), max(xs), max(ys))
        if bx[2] - bx[0] >= panel.width - 1e-3 and bx[3] - bx[1] >= panel.height - 1e-3:
            continue
        inner.append(bx)
    if len(inner) != 5:
        raise SystemExit(f"expected display window plus four mounting slots, found {len(inner)} inner contours")
    inner.sort(key=lambda b: (b[2] - b[0]) * (b[3] - b[1]), reverse=True)
    window, slots = inner[0], inner[1:]
    screws = sorted((((b[0] + b[2]) / 2, (b[1] + b[3]) / 2) for b in slots), key=lambda p: (round(p[1], 1), p[0]))
    return {
        "display": (window[0], window[1], window[2] - window[0], window[3] - window[1]),
        "encoder": encoder,
        "buttons": buttons,
        "inputs": sorted(inputs),
        "outputs": outputs,
        "leds": leds,
        "screws": screws,
        "button_hole": sizes[0],
    }


# --------------------------------------------------------------------------------------------
# Writers.
# --------------------------------------------------------------------------------------------

def fmt(v: float) -> str:
    text = f"{v:.4f}".rstrip("0").rstrip(".")
    return "0" if text in ("-0", "") else text


def _contour_path(contour: list[Segment]) -> str:
    out = [f"M{fmt(contour[0].points[0][0])} {fmt(contour[0].points[0][1])}"]
    for seg in contour:
        if seg.kind == "line":
            out.append(f"L{fmt(seg.points[-1][0])} {fmt(seg.points[-1][1])}")
        else:
            p1, p2, p3 = seg.points
            cx, cy = _arc_center(p1, p2, p3)
            r = math.hypot(p1[0] - cx, p1[1] - cy)
            cross = (p2[0] - p1[0]) * (p3[1] - p1[1]) - (p2[1] - p1[1]) * (p3[0] - p1[0])
            # Three-point arcs never exceed 180 degrees in this panel; large-arc flag stays 0.
            out.append(f"A{fmt(r)} {fmt(r)} 0 0 {1 if cross > 0 else 0} {fmt(p3[0])} {fmt(p3[1])}")
    out.append("Z")
    return " ".join(out)


def _circle_path(cx: float, cy: float, r: float) -> str:
    return (f"M{fmt(cx - r)} {fmt(cy)} A{fmt(r)} {fmt(r)} 0 1 0 {fmt(cx + r)} {fmt(cy)} "
            f"A{fmt(r)} {fmt(r)} 0 1 0 {fmt(cx - r)} {fmt(cy)} Z")


def panel_path_data(panel: Panel) -> str:
    parts = [_contour_path(c) for c in panel.contours]
    parts += [_circle_path(cx, cy, d / 2) for cx, cy, d in panel.circles]
    return " ".join(parts)


def engraving_elements(panel: Panel) -> list[str]:
    out = []
    for shape in panel.engraving:
        if shape[0] == "poly":
            _, pts, width, filled = shape
            d = "M" + " L".join(f"{fmt(x)} {fmt(y)}" for x, y in pts) + " Z"
            # KiCad splits glyphs into abutting or keyhole polygons. A 0.04 mm stroke in the fill
            # colour closes the anti-aliasing seam SVG renderers show along those joins; it grows
            # the outline by 0.02 mm, far below laser-engraving resolution.
            stroke = f' stroke-width="{fmt(width)}"' if width > 0 else f' stroke-width="{fmt(SEAM_STROKE_MM)}"'
            fill = "" if filled else ' fill="none"'
            out.append(f'<path d="{d}"{fill}{stroke}/>')
        elif shape[0] == "circle":
            _, (cx, cy), r, width, filled = shape
            fill = "" if filled else ' fill="none"'
            stroke = f' stroke-width="{fmt(width)}"' if width > 0 else ' stroke="none"'
            out.append(f'<circle cx="{fmt(cx)}" cy="{fmt(cy)}" r="{fmt(r)}"{fill}{stroke}/>')
        else:
            _, pts, width = shape
            d = "M" + " L".join(f"{fmt(x)} {fmt(y)}" for x, y in pts)
            out.append(f'<path d="{d}" fill="none" stroke-width="{fmt(width)}"/>')
    return out


def artwork_svg(panel: Panel) -> str:
    name = panel.source.name
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {fmt(panel.width)} {fmt(panel.height)}" '
        f'width="{fmt(panel.width)}mm" height="{fmt(panel.height)}mm" role="img" aria-labelledby="title desc">',
        f'  <title id="title">South Signal Lab CLOCK front panel, hardware Rev {panel.hw_rev}</title>',
        '  <desc id="desc">Front-panel artwork derived from the KiCad front-panel project: Edge.Cuts outline and '
        'cut-outs, F.SilkS engraving. Units are millimetres from the upper-left panel corner.</desc>',
        f'  <!-- GENERATED by scripts/import_kicad_front_panel.py from "{name}" '
        f'(HW_REV {panel.hw_rev}, sha256 {panel.sha256}). Do not edit by hand. -->',
        '  <defs>',
        f'    <clipPath id="panel-clip"><path clip-rule="evenodd" d="{panel_path_data(panel)}"/></clipPath>',
        '  </defs>',
        f'  <path id="panel" fill="{PANEL_COLOR}" fill-rule="evenodd" d="{panel_path_data(panel)}"/>',
        f'  <g id="engraving" clip-path="url(#panel-clip)" fill="{ENGRAVING_COLOR}" stroke="{ENGRAVING_COLOR}" '
        'stroke-linecap="round" stroke-linejoin="round">',
    ]
    lines += ["    " + e for e in engraving_elements(panel)]
    lines += ["  </g>", "</svg>"]
    return "\n".join(lines) + "\n"


def rasterize(panel: Panel, px_per_mm: float, holes: tuple | None):
    """Render the artwork with Pillow. holes=None gives transparent cut-outs (RGBA)."""
    from PIL import Image, ImageChops, ImageDraw

    s = px_per_mm * SUPERSAMPLE
    w = int(round(panel.width * px_per_mm))
    h = int(round(panel.height * px_per_mm))
    W, H = w * SUPERSAMPLE, h * SUPERSAMPLE
    P = lambda p: (p[0] * s, p[1] * s)

    panel_mask = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(panel_mask)
    outer = None
    for contour in panel.contours:
        pts = []
        for seg in contour:
            pts += seg.points if seg.kind == "line" else _arc_points(*seg.points)
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        if max(xs) - min(xs) >= panel.width - 1e-3 and max(ys) - min(ys) >= panel.height - 1e-3:
            outer = pts
    d.polygon([P(p) for p in outer], fill=255)
    for contour in panel.contours:
        pts = []
        for seg in contour:
            pts += seg.points if seg.kind == "line" else _arc_points(*seg.points)
        xs = [p[0] for p in pts]
        if max(xs) - min(xs) >= panel.width - 1e-3:
            continue
        d.polygon([P(p) for p in pts], fill=0)
    for cx, cy, dia in panel.circles:
        r = dia / 2 * s
        d.ellipse((cx * s - r, cy * s - r, cx * s + r, cy * s + r), fill=0)

    eng = Image.new("L", (W, H), 0)
    e = ImageDraw.Draw(eng)

    def stroke_polyline(pts, width, closed):
        wpx = max(1, int(round(width * s)))
        seq = pts + ([pts[0]] if closed else [])
        e.line([P(p) for p in seq], fill=255, width=wpx, joint="curve")
        rr = width * s / 2
        for p in seq:
            x, y = P(p)
            e.ellipse((x - rr, y - rr, x + rr, y + rr), fill=255)

    for shape in panel.engraving:
        if shape[0] == "poly":
            _, pts, width, filled = shape
            if filled:
                e.polygon([P(p) for p in pts], fill=255)
            if width > 0:
                stroke_polyline(pts, width, True)
        elif shape[0] == "circle":
            _, (cx, cy), r, width, filled = shape
            if filled:
                e.ellipse((cx * s - r * s, cy * s - r * s, cx * s + r * s, cy * s + r * s), fill=255)
            if width > 0:
                ro, ri = (r + width / 2) * s, max(0.0, (r - width / 2) * s)
                ring = Image.new("L", (W, H), 0)
                rd = ImageDraw.Draw(ring)
                rd.ellipse((cx * s - ro, cy * s - ro, cx * s + ro, cy * s + ro), fill=255)
                if not filled and ri > 0:
                    rd.ellipse((cx * s - ri, cy * s - ri, cx * s + ri, cy * s + ri), fill=0)
                eng = ImageChops.lighter(eng, ring)
                e = ImageDraw.Draw(eng)
        else:
            _, pts, width = shape
            stroke_polyline(pts, width, False)

    eng = ImageChops.multiply(eng, panel_mask)
    panel_mask = panel_mask.resize((w, h), Image.LANCZOS)
    eng = eng.resize((w, h), Image.LANCZOS)

    def rgb(hex_color):
        return tuple(int(hex_color[i:i + 2], 16) for i in (1, 3, 5))

    base = Image.new("RGB", (w, h), rgb(PANEL_COLOR))
    art = Image.composite(Image.new("RGB", (w, h), rgb(ENGRAVING_COLOR)), base, eng)
    if holes is None:
        out = art.convert("RGBA")
        out.putalpha(panel_mask)
        return out
    return Image.composite(art, Image.new("RGB", (w, h), holes), panel_mask)


def update_layout_ini(text: str, panel: Panel, elements: dict) -> str:
    def set_key(section: str, key: str, value: float) -> None:
        nonlocal text
        pattern = re.compile(r"(\[" + re.escape(section) + r"\][^\[]*?\n" + re.escape(key) + r" = )[^\n]*")
        text, n = pattern.subn(lambda m: m.group(1) + fmt(value), text)
        if n != 1:
            raise SystemExit(f"sim/panel_layout.ini: cannot update [{section}] {key}")

    set_key("panel", "width_mm", panel.width)
    set_key("panel", "height_mm", panel.height)
    dx, dy, dw, dh = elements["display"]
    for key, value in (("x_mm", dx), ("y_mm", dy), ("width_mm", dw), ("height_mm", dh)):
        set_key("display", key, value)
    set_key("encoder", "x_mm", elements["encoder"][0])
    set_key("encoder", "y_mm", elements["encoder"][1])
    for section, (x, y) in zip(("play", "tap", "stop"), elements["buttons"]):
        set_key(section, "x_mm", x)
        set_key(section, "y_mm", y)
    for section, (x, y) in zip(("sync", "reset"), elements["inputs"]):
        set_key(section, "x_mm", x)
        set_key(section, "y_mm", y)
    for index, ((x, y), (lx, ly)) in enumerate(zip(elements["outputs"], elements["leds"]), start=1):
        section = f"output_{index}"
        set_key(section, "x_mm", x)
        set_key(section, "y_mm", y)
        set_key(section, "led_x_mm", lx)
        set_key(section, "led_y_mm", ly)
    for index, (x, y) in enumerate(elements["screws"], start=1):
        set_key(f"screw_{index}", "x_mm", x)
        set_key(f"screw_{index}", "y_mm", y)

    marker = re.compile(r"# Front-panel source: .*\n")
    provenance = (f"# Front-panel source: {panel.source.name} (HW_REV {panel.hw_rev}, "
                  f"sha256 {panel.sha256[:16]}) via scripts/import_kicad_front_panel.py\n")
    if marker.search(text):
        text = marker.sub(provenance, text)
    else:
        raise SystemExit("sim/panel_layout.ini: missing '# Front-panel source:' provenance line")
    return text


def render_outputs(panel: Panel) -> dict[Path, bytes]:
    elements = classify(panel)
    png = io.BytesIO()
    rasterize(panel, PNG_PX_PER_MM, None).save(png, format="PNG", optimize=True)
    sim = io.BytesIO()
    rasterize(panel, SIM_PX_PER_MM, SIM_HOLE_COLOR).save(sim, format="PNG", optimize=True)
    ini = update_layout_ini(LAYOUT_INI.read_text(encoding="utf-8"), panel, elements)
    return {
        ARTWORK_SVG: artwork_svg(panel).encode("utf-8"),
        ARTWORK_PNG: png.getvalue(),
        SIM_BACKGROUND: sim.getvalue(),
        LAYOUT_INI: ini.encode("utf-8"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--kicad", type=Path, required=True,
                        help='path to "Simple Frontpanel.kicad_pcb" in the CLOCK hardware project')
    parser.add_argument("--check", action="store_true", help="fail if committed files differ from KiCad")
    args = parser.parse_args()

    panel = load_panel(args.kicad)
    outputs = render_outputs(panel)
    if args.check:
        stale = [str(p.relative_to(ROOT)) for p, data in outputs.items() if not p.exists() or p.read_bytes() != data]
        if stale:
            raise SystemExit("front-panel files differ from KiCad: " + ", ".join(stale))
        print(f"front panel matches {args.kicad.name} (HW_REV {panel.hw_rev})")
        return 0
    for path, data in outputs.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        print(path.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
