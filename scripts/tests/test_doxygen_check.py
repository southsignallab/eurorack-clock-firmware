"""Regression tests for the Doxygen warning baseline.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import json
import runpy
import shutil
import tempfile
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECK = runpy.run_path(str(ROOT / "scripts" / "check_doxygen.py"))
DOXYGEN = shutil.which("doxygen")

HEADER = """/**
 * @file sample.h
 * @brief Sample header.
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once
/** @brief Documented state. */
struct State {
    /** Documented field. */
    int documented;
%s};
"""


class DoxygenBaselineTests(unittest.TestCase):
    def test_warnings_are_grouped_by_source_directory(self) -> None:
        log = (
            f"{ROOT}/src/ui/menu_model.h:10: warning: Member x is not documented.\n"
            f"{ROOT}/src/config.h:3: warning: Member y is not documented.\n"
            "<unknown>: warning: Member main() (function) of file main.cpp is not documented.\n"
            f"{ROOT}/src/ui/ui_types.h:4: warning: Member z is not documented.\n"
            "<TIM3_IRQHandler>:1: warning: Member TIM3_IRQHandler() is not documented.\n"
        )
        self.assertEqual(CHECK["count_warnings"](log), Counter({"src/ui": 2, "src": 1, "(no file)": 2}))

    def test_more_warnings_than_the_baseline_fail(self) -> None:
        above, below = CHECK["compare"](Counter({"src/ui": 3}), {"src/ui": 2, "src/hal": 1})
        self.assertEqual(len(above), 1)
        self.assertEqual(len(below), 1)
        self.assertEqual(CHECK["compare"](Counter({"src/ui": 2}), {"src/ui": 2}), ([], []))

    def test_repository_baseline_is_zero_and_warnings_are_errors(self) -> None:
        # W-284: the tree has no Doxygen warning, so any new warning fails (Doxyfile and baseline).
        baseline = json.loads((ROOT / "scripts" / "doxygen_warning_baseline.json").read_text(encoding="utf-8"))
        self.assertEqual(sum(baseline.values()), 0)
        doxyfile = (ROOT / "Doxyfile").read_text(encoding="utf-8")
        self.assertRegex(doxyfile, r"(?m)^WARN_AS_ERROR\s*=\s*FAIL_ON_WARNINGS\s*$")

    @unittest.skipIf(DOXYGEN is None, "doxygen not installed")
    def test_undocumented_public_field_adds_a_warning(self) -> None:
        def warnings(extra: str) -> int:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / "src").mkdir()
                (root / "src" / "sample.h").write_text(HEADER % extra, encoding="utf-8")
                doxyfile = root / "Doxyfile"
                doxyfile.write_text((ROOT / "Doxyfile").read_text(encoding="utf-8").replace(
                    "INPUT                  = src lib/clock_core/src", "INPUT = src"), encoding="utf-8")
                log = CHECK["run_doxygen"](DOXYGEN, doxyfile, root)
                return sum(CHECK["count_warnings"](log, root).values())

        self.assertEqual(warnings(""), 0)
        self.assertEqual(warnings("    int undocumented;\n"), 1)


    def test_ci_uploads_the_doxygen_output(self) -> None:
        ci = (ROOT / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
        build = ci.index("run: doxygen Doxyfile")
        upload = ci.index("name: Upload Doxygen API documentation")
        self.assertLess(build, upload)
        step = ci[upload:upload + 300]
        self.assertIn("actions/upload-artifact@v7", step)
        self.assertIn("path: build/doxygen/", step)


if __name__ == "__main__":
    unittest.main()
