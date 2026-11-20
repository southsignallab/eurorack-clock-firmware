#!/usr/bin/env python3
"""Run Doxygen and fail when a directory has more warnings than its recorded baseline.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0

Doxygen runs with the repository `Doxyfile` into a temporary directory. Its warnings are counted
per source directory (`src/ui`, `src/engine`, ..., `src` for the top-level headers, `(no file)` for
global symbols) and compared with `scripts/doxygen_warning_baseline.json`. A directory with more
warnings than its baseline fails the check, so a new undocumented declaration, including a public
struct field, cannot slip in. Fewer warnings are reported so that the baseline can be lowered with
`--write-baseline`. When every count is zero, `WARN_AS_ERROR` can replace this check.

Usage (repository root):
  python scripts/check_doxygen.py                   # compare with the baseline
  python scripts/check_doxygen.py --write-baseline  # record the current counts
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT / "scripts" / "doxygen_warning_baseline.json"
# With WARN_AS_ERROR = FAIL_ON_WARNINGS, Doxygen prints its warnings as "error:".
WARNING_RE = re.compile(r"^(?P<path>.+?):(?P<line>\d+): (?:warning|error): ")
NO_FILE = "(no file)"


def warning_directory(path_text: str, root: Path = ROOT) -> str:
    """Return the baseline group of one warning location."""
    if path_text.startswith("<"):
        return NO_FILE  # global symbol without a documented file, for example <main>
    try:
        relative = Path(path_text).resolve().relative_to(root.resolve())
    except (ValueError, OSError):
        return NO_FILE
    parts = relative.parts
    if parts[:1] == ("src",) and len(parts) > 2:
        return f"src/{parts[1]}"
    if parts[:1] == ("lib",) and len(parts) > 1:
        return f"lib/{parts[1]}"
    return parts[0] if parts else NO_FILE


def count_warnings(log: str, root: Path = ROOT) -> Counter:
    """Count Doxygen warnings per baseline group."""
    counts: Counter = Counter()
    for line in log.splitlines():
        if "warning:" not in line and "error:" not in line:
            continue
        match = WARNING_RE.match(line)
        counts[warning_directory(match.group("path"), root) if match else NO_FILE] += 1
    return counts


def compare(current: Counter, baseline: dict[str, int]) -> tuple[list[str], list[str]]:
    """Return (groups above their baseline, groups below it) as readable lines."""
    groups = sorted(set(current) | set(baseline))
    above = [f"{g}: {current.get(g, 0)} warnings, baseline {baseline.get(g, 0)}"
             for g in groups if current.get(g, 0) > baseline.get(g, 0)]
    below = [f"{g}: {current.get(g, 0)} warnings, baseline {baseline.get(g, 0)}"
             for g in groups if current.get(g, 0) < baseline.get(g, 0)]
    return above, below


def run_doxygen(doxygen: str, doxyfile: Path = ROOT / "Doxyfile", cwd: Path = ROOT) -> str:
    """Run Doxygen into a temporary directory and return the warning log."""
    with tempfile.TemporaryDirectory() as tmp:
        log = Path(tmp) / "warnings.txt"
        overrides = f"\nOUTPUT_DIRECTORY = {tmp}\nWARN_LOGFILE = {log}\n"
        config = doxyfile.read_text(encoding="utf-8") + overrides
        result = subprocess.run([doxygen, "-"], input=config, cwd=cwd, capture_output=True, text=True)
        warnings = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        # WARN_AS_ERROR = FAIL_ON_WARNINGS: a non-zero exit with warnings is a counted result.
        if result.returncode != 0 and "warning:" not in warnings and "error:" not in warnings:
            raise RuntimeError(f"doxygen failed: {result.stderr.strip()}")
        return warnings


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--doxygen", default=shutil.which("doxygen") or "doxygen")
    parser.add_argument("--write-baseline", action="store_true", help="record the current counts")
    args = parser.parse_args(argv)
    try:
        counts = count_warnings(run_doxygen(args.doxygen))
    except (OSError, RuntimeError) as exc:
        print(f"Doxygen check failed: {exc}", file=sys.stderr)
        return 2
    if args.write_baseline:
        text = json.dumps(dict(sorted(counts.items())), indent=1) + "\n"
        BASELINE.write_bytes(text.encode("utf-8"))  # LF on every platform
        print(f"Doxygen warning baseline written: {sum(counts.values())} warnings")
        return 0
    above, below = compare(counts, json.loads(BASELINE.read_text(encoding="utf-8")))
    for line in below:
        print(f"fewer (lower the baseline): {line}")
    for line in above:
        print(f"MORE: {line}", file=sys.stderr)
    print(f"Doxygen: {sum(counts.values())} warnings, {len(above)} directories above baseline")
    return 1 if above else 0


if __name__ == "__main__":
    sys.exit(main())
