"""Regression tests for the clang-tidy baseline gate.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import runpy
import tempfile
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECK = runpy.run_path(str(ROOT / "scripts" / "check_static_analysis.py"))

OUTPUT = (
    f"{ROOT}/src/ui/a.cpp:10:5: warning: narrowing conversion [bugprone-narrowing-conversions]\n"
    f"{ROOT}/src/ui/a.cpp:20:5: warning: narrowing conversion [bugprone-narrowing-conversions]\n"
    "C:/msys64/ucrt64/include/c++/vector:1:1: warning: system [bugprone-foo]\n"
    "    int x = y;\n"
)


class StaticAnalysisBaselineTests(unittest.TestCase):
    def test_findings_are_counted_per_file_check_and_message(self) -> None:
        counts = CHECK["parse_findings"](OUTPUT)
        self.assertEqual(counts, Counter({("src/ui/a.cpp", "bugprone-narrowing-conversions", "narrowing conversion"): 2}))

    def test_a_new_finding_fails_and_a_moved_one_does_not(self) -> None:
        key = ("src/ui/a.cpp", "bugprone-narrowing-conversions", "narrowing conversion")
        new, gone = CHECK["compare"](Counter({key: 2}), Counter({key: 2}))
        self.assertEqual((new, gone), ([], []))
        new, _ = CHECK["compare"](Counter({key: 3}), Counter({key: 2}))
        self.assertEqual(len(new), 1)
        other = ("src/ui/b.cpp", "cert-err33-c", "ignored")
        new, _ = CHECK["compare"](Counter({key: 2, other: 1}), Counter({key: 2}))
        self.assertIn("src/ui/b.cpp", new[0])

    def test_fixed_findings_are_reported_but_pass(self) -> None:
        key = ("src/ui/a.cpp", "bugprone-narrowing-conversions", "narrowing conversion")
        new, gone = CHECK["compare"](Counter({key: 1}), Counter({key: 2}))
        self.assertEqual(new, [])
        self.assertEqual(len(gone), 1)

    def test_baseline_round_trip(self) -> None:
        key = ("src/ui/a.cpp", "cert-err33-c", "the value returned by this function should be used")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "baseline.json"
            CHECK["write_baseline"](Counter({key: 4}), "clang-tidy 23.1.4", path)
            self.assertEqual(CHECK["load_baseline"](path), Counter({key: 4}))

    def test_repository_baseline_and_profile_exist(self) -> None:
        self.assertTrue((ROOT / ".clang-tidy").is_file())
        self.assertGreater(sum(CHECK["load_baseline"]().values()), 0)


if __name__ == "__main__":
    unittest.main()
