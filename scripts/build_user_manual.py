#!/usr/bin/env python3
"""Publish a frozen CLOCK user-manual ODT as versioned ODT + PDF artifacts.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import tempfile
import re
import urllib.parse
from pathlib import Path, PurePath

import check_user_manual
import update_manual_contents

ROOT = Path(__file__).resolve().parents[1]
STABLE_VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
VERSION_RE = re.compile(r'^#define\s+CLOCK_FIRMWARE_VERSION\s+"([^"]+)"\s*$', re.MULTILINE)
# PDF export filter: keep embedded images at their own resolution. The default
# reduction to 300 dpi would resample the integer nearest-neighbour OLED
# screenshots and blur their pixel edges (D-12 point 5).
PDF_EXPORT_FILTER = 'pdf:writer_pdf_Export:{"ReduceImageResolution":{"type":"boolean","value":"false"}}'

def current_version() -> str:
    text = (ROOT / "src" / "version.h").read_text(encoding="utf-8")
    match = VERSION_RE.search(text)
    if match is None:
        raise RuntimeError("CLOCK_FIRMWARE_VERSION is missing from src/version.h")
    return match.group(1)

def default_source(version: str) -> Path:
    if STABLE_VERSION_RE.fullmatch(version):
        return ROOT / "docs" / "manual" / f"clock-user-manual.{version}.odt"
    return ROOT / "build" / "manual" / f"clock-user-manual.{version}.odt"


def libreoffice_binary() -> str:
    for candidate in ("libreoffice", "soffice"):
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    raise RuntimeError("LibreOffice/soffice is required to build the manual PDF")


def file_url(path: PurePath) -> str:
    """Return a file URL for an absolute POSIX or Windows path.

    Built from ``as_posix()`` so that a Windows path such as ``C:\\Temp`` becomes
    ``file:///C:/Temp`` instead of ``file://C:\\Temp``; ``PurePath.as_uri()`` is
    deprecated from Python 3.14.
    """
    if not path.is_absolute():
        raise ValueError(f"A file URL needs an absolute path: {path}")
    posix = path.as_posix()
    if posix.startswith("//"):
        return "file:" + urllib.parse.quote(posix, safe="/:")
    if not posix.startswith("/"):
        posix = "/" + posix
    return "file://" + urllib.parse.quote(posix, safe="/:")


def user_installation_argument(profile: PurePath) -> str:
    """Return the LibreOffice option that selects an isolated user profile."""
    return f"-env:UserInstallation={file_url(profile)}"


def build(source: Path, output_dir: Path, version: str, *, require_ubuntu_fonts: bool) -> tuple[Path, Path]:
    check_user_manual.validate_odt(source, version)
    if require_ubuntu_fonts:
        check_user_manual.validate_ubuntu_font_environment()
    output_dir.mkdir(parents=True, exist_ok=True)
    odt_out = output_dir / f"clock-user-manual.{version}.odt"
    pdf_out = output_dir / f"clock-user-manual.{version}.pdf"
    shutil.copy2(source, odt_out)

    with tempfile.TemporaryDirectory(prefix="clock-lo-profile-") as profile:
        command = [
            libreoffice_binary(),
            "--headless",
            user_installation_argument(Path(profile).resolve()),
            "--convert-to",
            PDF_EXPORT_FILTER,
            "--outdir",
            str(output_dir),
            str(odt_out),
        ]
        subprocess.run(command, check=True)

    if not pdf_out.is_file() or pdf_out.stat().st_size == 0:
        raise RuntimeError(f"LibreOffice did not produce {pdf_out}")

    # The post-1.1 publication manual uses a styled static chapter index rather
    # than LibreOffice's generated TOC. Resolve page numbers from the actual
    # exported artifact (and therefore the actual release fonts), patch the ODT,
    # then export once more. Page-number text itself does not alter pagination.
    if check_user_manual.requires_layout_v2(odt_out, version):
        update_manual_contents.update(odt_out, pdf_out)
        with tempfile.TemporaryDirectory(prefix="clock-lo-profile-toc-") as profile:
            command = [
                libreoffice_binary(),
                "--headless",
                user_installation_argument(Path(profile).resolve()),
                "--convert-to",
                PDF_EXPORT_FILTER,
                "--outdir",
                str(output_dir),
                str(odt_out),
            ]
            subprocess.run(command, check=True)

    check_user_manual.validate_odt(odt_out, version)
    check_user_manual.validate_pdf(pdf_out, version, require_ubuntu_fonts=require_ubuntu_fonts)
    return odt_out, pdf_out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default=current_version())
    parser.add_argument("--source", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    parser.add_argument("--require-ubuntu-fonts", action="store_true")
    args = parser.parse_args()
    source = args.source or default_source(args.version)
    odt, pdf = build(
        source.resolve(),
        args.output_dir.resolve(),
        args.version,
        require_ubuntu_fonts=args.require_ubuntu_fonts,
    )
    print(odt)
    print(pdf)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
