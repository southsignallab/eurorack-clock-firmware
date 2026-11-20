#!/usr/bin/env python3
"""Validate the frozen, user-facing release summary for one CLOCK version.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

#: Heading of the per-release list of known limitations.
KNOWN_ISSUES_HEADING = "## Known issues"
#: First firmware version whose summary must contain the Known issues section.
KNOWN_ISSUES_REQUIRED_FROM = (1, 2, 0)
MAXIMUM_KNOWN_ISSUES = 8
KNOWN_ISSUES_RE = re.compile(r"(?ms)^## Known issues[ \t]*\n(.*?)(?=^## |\Z)")
BULLET_RE = re.compile(r"(?m)^-\s+.+$")
VERSION_CORE_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)")


def requires_known_issues(version: str) -> bool:
    """True when the summary of ``version`` must contain the Known issues section."""
    match = VERSION_CORE_RE.match(version)
    if match is None:
        return False
    return tuple(int(part) for part in match.groups()) >= KNOWN_ISSUES_REQUIRED_FROM


def validate(path: Path, version: str) -> None:
    """Raise RuntimeError when the summary breaks the release-summary contract.

    Highlights and the other sections share the limit of 2-10 bullets. The Known issues
    section is checked on its own: 0-8 bullets, and without bullets it must state that none
    are known. It is required from firmware 1.2.0 on.
    """
    if not path.is_file():
        raise RuntimeError(f"Release summary missing: {path}")
    text = path.read_text(encoding="utf-8")
    if version not in text:
        raise RuntimeError(f"Release summary does not mention firmware {version}")
    if "## Highlights" not in text:
        raise RuntimeError("Release summary requires a '## Highlights' section")
    if re.search(r"\b(?:TODO|TBD|FIXME)\b", text, re.IGNORECASE):
        raise RuntimeError("Release summary still contains an unfinished placeholder")
    known = KNOWN_ISSUES_RE.search(text)
    if known is None and requires_known_issues(version):
        raise RuntimeError(f"Release summary requires a '{KNOWN_ISSUES_HEADING}' section from 1.2.0 on")
    known_body = known.group(1) if known else ""
    if len(BULLET_RE.findall(known_body)) > MAXIMUM_KNOWN_ISSUES:
        raise RuntimeError(f"Release summary Known issues must contain 0-{MAXIMUM_KNOWN_ISSUES} bullets")
    if known is not None and not known_body.strip():
        raise RuntimeError("Release summary Known issues must list the issues or state that none are known")
    remaining = text[:known.start()] + text[known.end():] if known else text
    bullets = BULLET_RE.findall(remaining)
    if not 2 <= len(bullets) <= 10:
        raise RuntimeError("Release summary must contain 2-10 concise user-facing bullets")
    if len(text.strip()) < 350:
        raise RuntimeError("Release summary is too short to explain the release")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("summary", type=Path)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    try:
        validate(args.summary.resolve(), args.version)
    except RuntimeError as exc:
        raise SystemExit(str(exc)) from exc
    print(f"Release summary check passed: {args.summary}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
