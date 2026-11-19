#!/usr/bin/env python3
# Author: Axel Napolitano
# License: PolyForm-Noncommercial-1.0.0
"""Validate that the public PlatformIO native command exposes the full native test inventory."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIO = ROOT / "platformio.ini"
SUITES = {
    "test_clock_core": ROOT / "test" / "test_clock_core" / "test_main.cpp",
    "test_realtime": ROOT / "test" / "test_realtime" / "test_main.cpp",
    "test_sync_behavior": ROOT / "test" / "test_sync_behavior" / "test_main.cpp",
    "test_sequencer2": ROOT / "test" / "test_sequencer2" / "test_main.cpp",
    "test_swing": ROOT / "test" / "test_swing" / "test_main.cpp",
    "test_humanize": ROOT / "test" / "test_humanize" / "test_main.cpp",
    "test_tap_tempo": ROOT / "test" / "test_tap_tempo" / "test_main.cpp",
    "test_controls": ROOT / "test" / "test_controls" / "test_main.cpp",
    "test_settings": ROOT / "test" / "test_settings" / "test_main.cpp",
    "test_screensavers": ROOT / "test" / "test_screensavers" / "test_main.cpp",
    "test_easter_eggs": ROOT / "test" / "test_easter_eggs" / "test_main.cpp",
    "test_host_firmware": ROOT / "test" / "test_host_firmware" / "test_main.cpp",
}
MIN_CASES = 360
MIN_SUITE_CASES = {
    "test_clock_core": 40,
    "test_realtime": 20,
    "test_sync_behavior": 75,
    "test_sequencer2": 14,
    "test_swing": 20,
    "test_humanize": 10,
    "test_tap_tempo": 20,
    "test_controls": 30,
    "test_settings": 50,
    "test_screensavers": 19,
    "test_easter_eggs": 20,
    "test_host_firmware": 20,
}


#: Documents that list the per-suite case counts. Each pattern pair finds the suite rows and
#: the stated total; the counts must equal the RUN_TEST registrations (F-076).
SUITE_ROW = re.compile(r"^\|\s*`(test_[a-z0-9_]+)`\s*\|\s*(\d+)\s*\|", re.MULTILINE)
INLINE_SUITE = re.compile(r"(\d+) `(test_[a-z0-9_]+)`")
COUNT_DOCUMENTS = (
    (ROOT / "test" / "README.md", SUITE_ROW, re.compile(r"\|\s*\*\*Total\*\*\s*\|\s*\*\*(\d+)\*\*")),
    (ROOT / "README.md", SUITE_ROW, re.compile(r"\*\*(\d+) explicitly named Native test cases\*\*")),
    (ROOT / "docs" / "DEVELOPMENT.md", INLINE_SUITE, re.compile(r"\*\*(\d+) named test cases\*\*")),
)


def documented_count_problems(
    name: str,
    text: str,
    suite_pattern: re.Pattern[str],
    total_pattern: re.Pattern[str],
    counts: dict[str, int],
    total: int,
) -> list[str]:
    """Compare the suite counts and the total stated in one document with the inventory."""
    problems: list[str] = []
    documented: dict[str, int] = {}
    for first, second in suite_pattern.findall(text):
        suite, value = (first, second) if first.startswith("test_") else (second, first)
        documented[suite] = int(value)
    for suite, count in counts.items():
        if suite not in documented:
            problems.append(f"{name}: no case count for {suite}")
        elif documented[suite] != count:
            problems.append(f"{name}: {suite} lists {documented[suite]} cases, inventory has {count}")
    for suite in sorted(set(documented) - set(counts)):
        problems.append(f"{name}: lists unknown suite {suite}")
    stated_totals = [int(value) for value in total_pattern.findall(text)]
    if not stated_totals:
        problems.append(f"{name}: total case count not found")
    for stated in stated_totals:
        if stated != total:
            problems.append(f"{name}: total lists {stated} cases, inventory has {total}")
    return problems


def main() -> int:
    config = PIO.read_text(encoding="utf-8")
    native_match = re.search(r"(?ms)^\[env:native\]\n(.*?)(?=^\[|\Z)", config)
    if native_match is None:
        raise RuntimeError("platformio.ini is missing [env:native]")
    native = native_match.group(1)
    if not re.search(r"(?m)^test_build_src\s*=\s*yes\s*$", native):
        raise RuntimeError("env:native must build production src for integration tests")
    if not re.search(r"(?m)^\s*-I\s+test/support\s*$", native):
        raise RuntimeError("env:native must expose test/support for shared native test headers")
    for suite in SUITES:
        if not re.search(rf"(?m)^\s+{re.escape(suite)}\s*$", native):
            raise RuntimeError(f"env:native does not expose suite: {suite}")

    total = 0
    counts: dict[str, int] = {}
    for suite, path in SUITES.items():
        text = path.read_text(encoding="utf-8")
        names = re.findall(r"RUN_TEST\((test[A-Za-z0-9_]+)\)", text)
        if len(names) != len(set(names)):
            raise RuntimeError(f"duplicate RUN_TEST registration in {suite}")
        counts[suite] = len(names)
        minimum = MIN_SUITE_CASES[suite]
        if len(names) < minimum:
            raise RuntimeError(
                f"native suite {suite} regressed to {len(names)} cases; minimum is {minimum}"
            )
        total += len(names)

    if total < MIN_CASES:
        raise RuntimeError(f"native test inventory regressed to {total}; minimum is {MIN_CASES}")
    problems: list[str] = []
    for path, suite_pattern, total_pattern in COUNT_DOCUMENTS:
        problems.extend(
            documented_count_problems(
                path.relative_to(ROOT).as_posix(),
                path.read_text(encoding="utf-8"),
                suite_pattern,
                total_pattern,
                counts,
                total,
            )
        )
    if problems:
        raise RuntimeError("documented native test counts are stale:\n  " + "\n  ".join(problems))
    print("Native test inventory: " + ", ".join(f"{k}={v}" for k, v in counts.items()) + f", total={total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
