"""Regression tests for the static UI text check in check_architecture.py.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import runpy
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECK = runpy.run_path(str(ROOT / "scripts" / "check_architecture.py"))
static_ui_text_lines = CHECK["static_ui_text_lines"]


class StaticUiTextTests(unittest.TestCase):
    def test_literal_in_code_is_reported_with_its_line(self) -> None:
        source = 'void f() {\n    draw("PLAY");\n}\n'
        self.assertEqual(static_ui_text_lines(source), [2])

    def test_includes_comments_and_single_line_static_assert_are_ignored(self) -> None:
        source = (
            '#include "ui/menu_model.h"\n'
            '// "SAVE" appears only in a comment\n'
            'static_assert(kSize == 4, "Size must be four");\n'
        )
        self.assertEqual(static_ui_text_lines(source), [])

    def test_message_of_a_wrapped_static_assert_is_ignored(self) -> None:
        source = (
            "static_assert(\n"
            "    kModules * kScale + 2 * (kQrX - kFieldX) == kFieldSize,\n"
            '    "QR must be centred in its field");\n'
            'draw("STOP");\n'
        )
        self.assertEqual(static_ui_text_lines(source), [4])


if __name__ == "__main__":
    unittest.main()
