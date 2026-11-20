"""Regression tests for the inline cursor-modulo rule in check_architecture.py (F-004 class).

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import runpy
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECK = runpy.run_path(str(ROOT / "scripts" / "check_architecture.py"))
cursor_modulo_lines = CHECK["cursor_modulo_lines"]


class CursorModuloTests(unittest.TestCase):
    def test_signed_cursor_wrap_is_reported(self) -> None:
        source = "void f() {\n    cursor = static_cast<std::uint8_t>((cursor + count + delta) % count);\n}\n"
        self.assertEqual(cursor_modulo_lines(source), [2])

    def test_member_cursor_wrap_on_a_continuation_line_is_reported(self) -> None:
        source = (
            "navigation_.grooveCursor = static_cast<std::uint8_t>(\n"
            "    (navigation_.grooveCursor + 1U) % navigation_.grooveDraft.length);\n"
        )
        self.assertEqual(cursor_modulo_lines(source), [2])

    def test_wrap_helper_and_unrelated_modulo_pass(self) -> None:
        source = (
            "cursor = wrapIndex(cursor, delta, count);\n"
            "const std::size_t next = (index + 1U) % kCurveProfile.size();\n"
            "// (cursor + delta) % count is the F-004 class\n"
            "navigation_.sequencerCursor = static_cast<std::uint8_t>(absoluteStep % 8);\n"
        )
        self.assertEqual(cursor_modulo_lines(source), [])


if __name__ == "__main__":
    unittest.main()
