#!/usr/bin/env python3
"""Read and validate firmware version metadata.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

VERSION_RE = re.compile(r'^#define\s+CLOCK_FIRMWARE_VERSION\s+"([^"]+)"\s*$')


def read_define(path: Path, pattern: re.Pattern[str]) -> str:
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line.strip())
        if match:
            return match.group(1)
    raise SystemExit(f"Required version define not found in {path}")


def git_commit(revision: str) -> str | None:
    """Return the commit a revision names, or None when Git does not know it."""
    result = subprocess.run(
        ["git", "rev-parse", "--quiet", "--verify", f"{revision}^{{commit}}"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    return result.stdout.strip() if result.returncode == 0 else None


def released_version_conflict(version: str) -> str | None:
    """Explain why this source must not be released as *version*, or return None.

    A stable version that already has its release tag can only be reproduced
    from that tag: the release path regenerates the manual screenshots from the
    checked-out firmware, so any later commit would put screenshots of other
    firmware into the frozen manual of the released version. Prerelease
    versions are never frozen and pass.
    """
    if "-" in version:
        return None
    tag = f"v{version}"
    tagged = git_commit(f"refs/tags/{tag}")
    if tagged is None or tagged == git_commit("HEAD"):
        return None
    return (
        f"Version {version} is already released as tag {tag} (commit {tagged[:12]}). "
        f"Rebuild that release from its tag, or raise CLOCK_FIRMWARE_VERSION in src/version.h "
        f"before releasing this source."
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--file", default="src/version.h")
    parser.add_argument("--check-tag", help="Validate a Git tag, e.g. v1.2.3")
    parser.add_argument(
        "--require-unreleased",
        action="store_true",
        help="Fail when a stable version already has its release tag on another commit",
    )
    args = parser.parse_args()

    path = Path(args.file)
    value = read_define(path, VERSION_RE)

    if args.check_tag:
        expected = f"v{value}"
        if args.check_tag != expected:
            print(f"Version mismatch: tag={args.check_tag!r}, firmware={expected!r}", file=sys.stderr)
            return 2

    if args.require_unreleased:
        conflict = released_version_conflict(value)
        if conflict is not None:
            print(conflict, file=sys.stderr)
            return 3

    print(value)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
