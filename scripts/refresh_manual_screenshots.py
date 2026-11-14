#!/usr/bin/env python3
"""Refresh every generated OLED screenshot embedded in the CLOCK ODT manual.

The release manual deliberately keeps its page geometry and frame placement in ODT.
This tool replaces the image payloads behind those fixed frames with screenshots
rendered from the current firmware and rebuilds the generated screenshot galleries.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import argparse
import shutil
import tempfile
import zipfile
from pathlib import Path

from lxml import etree

import update_manual_galleries

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT / "docs" / "manual-source" / "clock-user-manual.odt"
DEFAULT_ASSETS = ROOT / "docs" / "manual-source" / "assets"

NS = {
    "draw": "urn:oasis:names:tc:opendocument:xmlns:drawing:1.0",
    "xlink": "http://www.w3.org/1999/xlink",
}

# Stable ODT frame names are the publication contract. The manual embeds the
# print variants (dark on light, framed by the display cut-out) written by
# scripts/generate_manual_screenshots.py.  The archive-side image
# names are intentionally irrelevant: LibreOffice may rename them at any time.
DIRECT_SCREENSHOTS: dict[str, str] = {
    "ManualImage2": "boot-1000-print.png",
    "ManualImage3": "performance-one-clock-play-print.png",
    "ManualImage4": "performance-independent-clock-play-print.png",
    "ManualImage5": "performance-independent-euclid-play-print.png",
    "ManualImage6": "performance-independent-sequencer-play-print.png",
    "ManualImage7": "channel-overview-independent-print.png",
    "ManualImage8": "mode-select-euclid-print.png",
    "ManualImage9": "mode-change-confirm-no-print.png",
    "ManualImage10": "settings-euclid-print.png",
    "ManualImage11": "sequencer-editor-print.png",
    "ManualImage12": "settings-one-clock-timing-print.png",
    "ManualImage13": "performance-divider-bank-play-print.png",
    "ManualImage14": "settings-root-print.png",
    "ManualImage15": "settings-input-config-print.png",
    "ManualImage16": "preset-name-entry-print.png",
    "ManualImage17": "settings-screensaver-print.png",
    "ManualImage18": "arcade-name-entry-print.png",
    "ManualImage19": "beatknecht-print.png",
    "ManualImage20": "groove-editor-custom-print.png",
    "ManualImage21": "groove-record-live-print.png",
    "ManualImage22": "performance-pre-count-progress-print.png",
    "ManualImage23": "settings-licenses-print.png",
    "ManualImage24": "settings-general-print.png",
    "ManualImage25": "settings-master-print.png",
}

# Manuals up to 1.1.0 (full-page cover images) keep their original contract:
# screen variants in the frames of that layout.
LEGACY_DIRECT_SCREENSHOTS: dict[str, str] = {
    "ManualImage2": "boot-1000.png",
    "ManualImage3": "performance-one-clock-play.png",
    "ManualImage4": "performance-independent-clock-play.png",
    "ManualImage5": "performance-independent-euclid-play.png",
    "ManualImage6": "performance-independent-sequencer-play.png",
    "ManualImage7": "channel-overview-independent.png",
    "ManualImage8": "mode-select-euclid.png",
    "ManualImage9": "mode-change-confirm-no.png",
    "ManualImage10": "performance-independent-euclid-play.png",
    "ManualImage11": "sequencer-editor.png",
    "ManualImage12": "performance-one-clock-play.png",
    "ManualImage13": "performance-divider-bank-play.png",
    "ManualImage14": "settings-root.png",
    "ManualImage15": "settings-input-config.png",
    "ManualImage16": "preset-name-entry.png",
    "ManualImage17": "settings-screensaver.png",
    "ManualImage18": "arcade-top-100.png",
    "ManualImage19": "beatknecht.png",
    "ManualImage20": "groove-editor-custom.png",
    "ManualImage21": "groove-record-live.png",
    "ManualImage22": "performance-pre-count-progress.png",
    "ManualImage23": "settings-licenses.png",
    "ManualImage24": "settings-general.png",
    "ManualImage25": "settings-master.png",
}

# Generated line drawings embedded in the manual: frame name -> asset. The
# refresh step replaces their payloads like the screenshots, so the printed
# panel can never drift from the KiCad-derived artwork.
DIRECT_ILLUSTRATIONS: dict[str, str] = {
    "ManualImage1": "front-panel-anatomy-print.svg",
    "ManualCoverPanel": "front-panel-print.svg",
}


def generated_asset_names(assets: Path) -> set[str]:
    manifest = assets / "manual-screenshots.tsv"
    if not manifest.is_file():
        raise FileNotFoundError(f"Missing generated screenshot manifest: {manifest}")
    names: set[str] = set()
    for raw in manifest.read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            continue
        # name<TAB>print name<TAB>description: both variants are generated assets.
        for name in raw.split("\t")[:-1]:
            names.add(f"{name}.png")
    return names


def is_typeset_manual(source: Path) -> bool:
    """Return whether *source* has the typeset cover of D-12 (print screenshots)."""
    with zipfile.ZipFile(source, "r") as archive:
        return b'draw:name="ManualCoverPanel"' in archive.read("content.xml")


def frame_contract(source: Path) -> dict[str, str]:
    """Return frame name -> asset for the layout generation of *source*."""
    if is_typeset_manual(source):
        return {**DIRECT_SCREENSHOTS, **DIRECT_ILLUSTRATIONS}
    return dict(LEGACY_DIRECT_SCREENSHOTS)


def direct_frame_targets(source: Path, assets: Path) -> dict[str, tuple[str, Path]]:
    """Map every refreshed frame (screenshots and line drawings) to its href and asset."""
    generated = generated_asset_names(assets)
    contract = frame_contract(source)
    missing_assets = sorted({name for name in contract.values() if name.endswith(".png")} - generated)
    if missing_assets:
        raise RuntimeError(
            "Direct manual screenshot contract references assets not emitted by the current generator: "
            + ", ".join(missing_assets)
        )

    with zipfile.ZipFile(source, "r") as archive:
        root = etree.fromstring(archive.read("content.xml"))
        frames = root.xpath("//draw:frame", namespaces=NS)
        targets: dict[str, tuple[str, Path]] = {}
        seen: set[str] = set()
        for frame in frames:
            frame_name = frame.get(f"{{{NS['draw']}}}name")
            if frame_name not in contract:
                continue
            if frame_name in seen:
                raise RuntimeError(f"Manual frame occurs more than once: {frame_name}")
            seen.add(frame_name)
            images = frame.xpath("./draw:image", namespaces=NS)
            if len(images) != 1:
                raise RuntimeError(f"Manual frame {frame_name} must contain exactly one image")
            href = images[0].get(f"{{{NS['xlink']}}}href")
            if not href or not href.startswith("Pictures/"):
                raise RuntimeError(f"Manual frame {frame_name} has an invalid embedded image href: {href!r}")
            asset = assets / contract[frame_name]
            if not asset.is_file():
                raise FileNotFoundError(f"Missing generated manual asset: {asset}")
            targets[frame_name] = (href, asset)

    missing_frames = sorted(set(contract) - seen)
    if missing_frames:
        raise RuntimeError(
            "Manual screenshot frames are missing from the publication ODT: "
            + ", ".join(missing_frames)
        )
    return targets


def replace_direct_screenshots(source: Path, destination: Path, assets: Path) -> int:
    targets = direct_frame_targets(source, assets)
    replacements = {href: asset.read_bytes() for href, asset in targets.values()}
    destination.parent.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(source, "r") as archive:
        entries = [(info, archive.read(info.filename)) for info in archive.infolist()]

    with tempfile.NamedTemporaryFile(suffix=".odt", delete=False, dir=destination.parent) as temp:
        temporary = Path(temp.name)
    try:
        with zipfile.ZipFile(temporary, "w") as output:
            for info, original in entries:
                payload = replacements.get(info.filename, original)
                compression = zipfile.ZIP_STORED if info.filename == "mimetype" else info.compress_type
                output.writestr(info, payload, compress_type=compression)
        shutil.move(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    return len(targets)


def verify_refreshed_manual(manual: Path, assets: Path) -> tuple[int, int]:
    targets = direct_frame_targets(manual, assets)
    typeset = is_typeset_manual(manual)
    direct_count = 0
    gallery_count = 0
    with zipfile.ZipFile(manual, "r") as archive:
        for frame_name, (href, asset) in targets.items():
            if archive.read(href) != asset.read_bytes():
                raise RuntimeError(
                    f"Embedded screenshot payload is stale after refresh: {frame_name} -> {asset.name}"
                )
            direct_count += 1

        gallery_sets = (
            ("ManualGalleryScreensavers", update_manual_galleries.SCREENSAVER_ROWS),
            ("ManualGalleryEasterEggs", update_manual_galleries.EASTER_ROWS),
        )
        for gallery_name, rows in gallery_sets:
            for row_number, row_items in enumerate(rows, 1):
                for column_number, (filename, _caption) in enumerate(row_items, 1):
                    archive_name = (
                        f"Pictures/gallery_{gallery_name.lower()}_"
                        f"{row_number:02d}_{column_number:02d}.png"
                    )
                    asset_name = update_manual_galleries.print_variant(filename) if typeset else filename
                    expected = assets / asset_name
                    if archive.read(archive_name) != expected.read_bytes():
                        raise RuntimeError(
                            f"Embedded gallery screenshot is stale after refresh: {archive_name}"
                        )
                    gallery_count += 1
    return direct_count, gallery_count


def refresh(source: Path, destination: Path, assets: Path) -> tuple[int, int]:
    replace_direct_screenshots(source, destination, assets)
    update_manual_galleries.rebuild_odt(destination, assets, print_variants=is_typeset_manual(destination))
    return verify_refreshed_manual(destination, assets)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--assets", type=Path, default=DEFAULT_ASSETS)
    args = parser.parse_args()
    direct_count, gallery_count = refresh(
        args.source.resolve(), args.output.resolve(), args.assets.resolve()
    )
    print(
        f"Refreshed {direct_count} direct OLED screenshots and "
        f"{gallery_count} gallery screenshots in {args.output.resolve()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
