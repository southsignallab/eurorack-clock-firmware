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

    def test_paths_that_are_not_absolute_on_this_platform_are_ignored(self) -> None:
        # CI on Linux: "C:/msys64/..." is a relative path there and resolved into the repository.
        output = "include/c++/vector:1:1: warning: system [bugprone-foo]\n"
        self.assertEqual(CHECK["parse_findings"](output), Counter())

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

    def test_gate_is_mandatory_in_ci_and_in_the_local_chain(self) -> None:
        # W-117: new findings fail CI; the job uses the same pinned tool and target as the local check.
        ci = (ROOT / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
        self.assertIn('python -m pip install "clang-tidy==23.1.4"', ci)
        self.assertIn("python scripts/check_static_analysis.py --clang-tidy clang-tidy", ci)
        self.assertIn("--extra-arg=--target=x86_64-w64-windows-gnu", ci)
        contributing = (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8")
        chain = contributing.split("## Local verification", 1)[1].split("Target builds:", 1)[0]
        self.assertIn("python scripts/check_static_analysis.py", chain)
        self.assertNotIn("not yet part of CI", contributing)

    def test_baseline_is_below_the_first_recorded_baseline(self) -> None:
        # W-117: the baseline written after the clean-up must be smaller than the 342 of the first one.
        self.assertLess(sum(CHECK["load_baseline"]().values()), 342)


if __name__ == "__main__":
    unittest.main()
