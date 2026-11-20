"""Regression tests for the host-only framework include rule in check_architecture.py.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import runpy
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECK = runpy.run_path(str(ROOT / "scripts" / "check_architecture.py"))
unguarded_framework_includes = CHECK["unguarded_framework_includes"]

HOST = "#if defined(CLOCK_HOST_TEST) || defined(CLOCK_SIMULATOR)\n"


class HostPathIncludeTests(unittest.TestCase):
    def test_include_in_the_host_branch_passes(self) -> None:
        source = HOST + "#include <Arduino.h>\n#else\n#include <stm32f4xx_hal.h>\n#endif\n"
        self.assertEqual(unguarded_framework_includes(source), [])

    def test_include_without_guard_is_reported(self) -> None:
        self.assertEqual(unguarded_framework_includes("#include <Wire.h>\n"), [1])

    def test_include_in_the_target_branch_is_reported(self) -> None:
        source = HOST + "#include <SPI.h>\n#else\n#include <HardwareTimer.h>\n#endif\n"
        self.assertEqual(unguarded_framework_includes(source), [4])

    def test_nested_condition_inside_the_host_branch_passes(self) -> None:
        source = HOST + "#if defined(CLOCK_HOST_TEST)\n#include <Arduino.h>\n#endif\n#endif\n"
        self.assertEqual(unguarded_framework_includes(source), [])

    def test_config_header_stops_a_target_build_with_the_host_path(self) -> None:
        config = (ROOT / "src" / "config.h").read_text(encoding="utf-8")
        self.assertIn("#if (defined(CLOCK_HOST_TEST) || defined(CLOCK_SIMULATOR)) && defined(STM32F4xx)", config)
        self.assertIn("#error", config)


if __name__ == "__main__":
    unittest.main()
