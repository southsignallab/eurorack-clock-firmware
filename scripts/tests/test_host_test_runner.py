"""Tests for the build-tree handling of the host-test runner.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]


def load_runner():
    spec = importlib.util.spec_from_file_location("run_host_tests", ROOT / "scripts" / "run_host_tests.py")
    if spec is None or spec.loader is None:
        raise AssertionError("Unable to load scripts/run_host_tests.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class HostTestBuildTreeTests(unittest.TestCase):
    """F-052: concurrent runs never delete each other's build tree (W-304)."""

    def setUp(self) -> None:
        self.runner = load_runner()
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        host_tests = Path(self.directory.name) / "build" / "host_tests"
        # Older and newer runner layouts: one shared tree or one tree per mode below it.
        for name in ("BUILD_ROOT", "HOST_TESTS_ROOT"):
            if hasattr(self.runner, name):
                patcher = mock.patch.object(self.runner, name, host_tests)
                patcher.start()
                self.addCleanup(patcher.stop)
        # No compiler runs: every suite step only records into the current build tree.
        for name in ("audit_production_stack_frames", "collect_coverage", "write_coverage_report", "enforce_thresholds"):
            patcher = mock.patch.object(self.runner, name, return_value={})
            patcher.start()
            self.addCleanup(patcher.stop)
        for name in dir(self.runner):
            if name.startswith("compile_") and name not in {"compile_sanitized_firmware_variant"}:
                patcher = mock.patch.object(self.runner, name, side_effect=self.record_build)
                patcher.start()
                self.addCleanup(patcher.stop)
        self.while_running = None
        patcher = mock.patch.object(self.runner, "run_sanitizer_matrix", side_effect=self.sanitizer_matrix)
        patcher.start()
        self.addCleanup(patcher.stop)

    def record_build(self, *args, **kwargs) -> Path:
        build_dir = self.runner.BUILD_ROOT / "suite"
        build_dir.mkdir(parents=True, exist_ok=True)
        (build_dir / "tests").write_text("binary", encoding="utf-8")
        return build_dir

    def sanitizer_matrix(self) -> None:
        # Only this run writes the marker, so a concurrent run cannot recreate it.
        marker = self.record_build() / "sanitizer-run-in-progress"
        marker.write_text("running", encoding="utf-8")
        if self.while_running is not None:
            self.while_running()
        self.assertTrue(marker.is_file(), "a concurrent run deleted this run's build tree")

    def run_main(self, *arguments: str) -> int:
        with mock.patch.object(sys, "argv", ["run_host_tests.py", *arguments]):
            return self.runner.main()

    def test_coverage_run_started_during_a_sanitizer_run_keeps_the_sanitizer_tree(self) -> None:
        results: list[int] = []
        self.while_running = lambda: results.append(self.run_main("--skip-sanitizers"))
        self.assertEqual(0, self.run_main("--sanitizers-only"))
        self.assertEqual([0], results)

    def test_second_run_of_the_same_mode_stops_with_a_clear_message(self) -> None:
        results: list[int] = []
        messages: list[str] = []

        def second_run() -> None:
            self.while_running = None
            with mock.patch("sys.stderr") as stderr:
                results.append(self.run_main("--sanitizers-only"))
            messages.extend(call.args[0] for call in stderr.write.call_args_list)

        self.while_running = second_run
        self.assertEqual(0, self.run_main("--sanitizers-only"))
        self.assertEqual([2], results)
        message = "".join(messages)
        self.assertIn("another run_host_tests.py run", message)
        self.assertIn("sanitizers", message)

    def test_runs_of_the_same_mode_one_after_another_both_finish(self) -> None:
        self.assertEqual(0, self.run_main("--sanitizers-only"))
        self.assertEqual(0, self.run_main("--sanitizers-only"))

    def test_each_mode_uses_its_own_build_tree(self) -> None:
        trees = set()
        for arguments in ((), ("--skip-sanitizers",), ("--sanitizers-only",), ("--focus", "core")):
            self.run_main(*arguments)
            trees.add(self.runner.BUILD_ROOT)
        # The full run and the coverage-only run share the coverage tree.
        self.assertEqual(3, len(trees))


if __name__ == "__main__":
    unittest.main()
