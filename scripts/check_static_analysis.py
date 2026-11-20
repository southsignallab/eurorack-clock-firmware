#!/usr/bin/env python3
"""Run clang-tidy with the project profile and fail on findings that are not in the baseline.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0

The profile is `.clang-tidy`. The check analyzes the host configuration of every implementation
file in `src/` and `lib/clock_core/src/` (the flags of `env:native`), so target-only branches
are not covered. Findings are compared by file, check and message, not by line, so that edits
elsewhere in a file do not count as new findings. A finding that is not in the baseline fails
the check; findings that disappeared are reported so the baseline can be lowered with
`--write-baseline`.

Usage (repository root):
  python scripts/check_static_analysis.py --clang-tidy <path>                  # compare
  python scripts/check_static_analysis.py --clang-tidy <path> --write-baseline # record
  Extra compiler arguments (for example a target triple and sysroot on Windows):
  --extra-arg=--target=x86_64-w64-windows-gnu --extra-arg=--sysroot=C:/msys64/ucrt64
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT / "scripts" / "static_analysis_baseline.json"
HOST_FLAGS = (
    "-std=c++17",
    "-DCLOCK_HOST_TEST=1",
    "-DCLOCK_NATIVE_SMOKE_BUILD=1",
    "-Isrc",
    "-Ilib/clock_core/src",
    "-Itest/support",
    "-Itest/support/fake_framework",
)
FINDING_RE = re.compile(r"^(.*?):(\d+):(\d+): (?:warning|error): (.*) \[([\w.,-]+)\]$")

Key = tuple[str, str, str]


def sources(root: Path = ROOT) -> list[Path]:
    """Return the translation units the check analyzes."""
    return sorted([*(root / "src").rglob("*.cpp"), *(root / "lib" / "clock_core" / "src").rglob("*.cpp")])


def compile_database(files: list[Path], extra: list[str], root: Path = ROOT) -> list[dict]:
    """Return a compile database for the host configuration of ``files``."""
    return [
        {"directory": str(root), "file": str(path), "arguments": ["clang++", *extra, *HOST_FLAGS, "-c", str(path)]}
        for path in files
    ]


def parse_findings(output: str, root: Path = ROOT) -> Counter:
    """Count clang-tidy findings by repository-relative file, check and message."""
    counts: Counter = Counter()
    for line in output.splitlines():
        match = FINDING_RE.match(line.strip())
        if match is None:
            continue
        path, _, _, message, check = match.groups()
        if not Path(path).is_absolute():
            # clang-tidy prints absolute paths. A path that is not absolute here comes from
            # another platform (C:/msys64/... on Linux) and would resolve into the repository.
            continue
        try:
            relative = Path(path).resolve().relative_to(root.resolve()).as_posix()
        except ValueError:
            continue  # system or framework header
        counts[(relative, check, message)] += 1
    return counts


def compare(current: Counter, baseline: Counter) -> tuple[list[str], list[str]]:
    """Return (new findings, findings no longer present) as readable lines."""
    new = [f"{f}: [{c}] {m} (now {current[(f, c, m)]}, baseline {baseline.get((f, c, m), 0)})"
           for (f, c, m) in sorted(current) if current[(f, c, m)] > baseline.get((f, c, m), 0)]
    gone = [f"{f}: [{c}] {m} (now {current.get((f, c, m), 0)}, baseline {baseline[(f, c, m)]})"
            for (f, c, m) in sorted(baseline) if current.get((f, c, m), 0) < baseline[(f, c, m)]]
    return new, gone


def load_baseline(path: Path = BASELINE) -> Counter:
    """Read the baseline file."""
    data = json.loads(path.read_text(encoding="utf-8"))
    return Counter({(e["file"], e["check"], e["message"]): int(e["count"]) for e in data["findings"]})


def write_baseline(counts: Counter, tool_version: str, path: Path = BASELINE) -> None:
    """Write the baseline file, sorted for stable diffs."""
    findings = [{"file": f, "check": c, "message": m, "count": n} for (f, c, m), n in sorted(counts.items())]
    text = json.dumps({"clang_tidy": tool_version, "findings": findings}, indent=1) + "\n"
    path.write_bytes(text.encode("utf-8"))  # LF on every platform


def run_clang_tidy(clang_tidy: str, extra: list[str], jobs: int) -> Counter:
    """Run clang-tidy over every translation unit and count the findings."""
    files = sources()
    with tempfile.TemporaryDirectory() as tmp:
        (Path(tmp) / "compile_commands.json").write_text(json.dumps(compile_database(files, extra)),
                                                        encoding="utf-8")

        def one(path: Path) -> str:
            result = subprocess.run(
                [clang_tidy, f"-p={tmp}", f"--config-file={ROOT / '.clang-tidy'}", "--quiet", str(path)],
                cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
            return result.stdout

        with ThreadPoolExecutor(jobs) as pool:
            outputs = list(pool.map(one, files))
    counts: Counter = Counter()
    for output in outputs:
        counts.update(parse_findings(output))
    return counts


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--clang-tidy", default="clang-tidy", help="clang-tidy executable (23.1.4)")
    parser.add_argument("--extra-arg", action="append", default=[], help="extra compiler argument")
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--write-baseline", action="store_true", help="record the current findings")
    args = parser.parse_args(argv)
    version = subprocess.run([args.clang_tidy, "--version"], capture_output=True, text=True).stdout
    tool_version = next((line.strip() for line in version.splitlines() if "version" in line), "unknown")
    current = run_clang_tidy(args.clang_tidy, args.extra_arg, args.jobs)
    if args.write_baseline:
        write_baseline(current, tool_version)
        print(f"static analysis baseline written: {sum(current.values())} findings")
        return 0
    new, gone = compare(current, load_baseline())
    for line in gone:
        print(f"fixed (lower the baseline): {line}")
    for line in new:
        print(f"NEW: {line}", file=sys.stderr)
    print(f"static analysis: {sum(current.values())} findings, {len(new)} new, {len(gone)} fewer than baseline")
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main())
