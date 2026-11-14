#!/usr/bin/env python3
"""Regression tests for the retired documentation footer.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""

from __future__ import annotations

import importlib.util
import re
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/check_documentation.py'
spec = importlib.util.spec_from_file_location('check_documentation', SCRIPT)
assert spec is not None and spec.loader is not None
check_documentation = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = check_documentation
spec.loader.exec_module(check_documentation)

RETIRED_SENTENCE = 'From Munich with'
RUNNING_MANUAL = ROOT / 'docs/manual-source/clock-user-manual.odt'


class RetiredFooterTests(unittest.TestCase):
    """The former centered footer must not return to current documentation."""

    def test_check_reports_retired_footer_in_current_document(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'README.md'
            path.write_text('# Title\n\n<h6 align="center">From Munich with &#9829;</h6>\n', encoding='utf-8')
            errors: list[str] = []
            check_documentation.check_retired_footer(errors, [path])
            self.assertEqual(1, len(errors))
            self.assertIn('retired footer', errors[0])

    def test_check_accepts_document_without_footer(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'README.md'
            path.write_text('# Title\n\nBody text.\n', encoding='utf-8')
            errors: list[str] = []
            check_documentation.check_retired_footer(errors, [path])
            self.assertEqual([], errors)

    def test_current_repository_documents_carry_no_retired_footer(self) -> None:
        errors: list[str] = []
        check_documentation.check_retired_footer(errors)
        self.assertEqual([], errors)

    def test_running_manual_footer_has_title_and_page_number_only(self) -> None:
        with zipfile.ZipFile(RUNNING_MANUAL) as archive:
            styles = archive.read('styles.xml').decode('utf-8')
            content = archive.read('content.xml').decode('utf-8')
        self.assertNotIn(RETIRED_SENTENCE, styles)
        self.assertNotIn(RETIRED_SENTENCE, content)
        footers = re.findall(r'<style:footer>(.*?)</style:footer>', styles, re.S)
        labelled = [footer for footer in footers if 'CLOCK User Manual' in footer]
        self.assertEqual(1, len(labelled))
        self.assertIn('<text:page-number', labelled[0])
        self.assertNotIn('<draw:frame', labelled[0])

    def test_running_manual_check_reports_retired_footer(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'manual.odt'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('content.xml', '<text:p>From Munich with </text:p>')
                archive.writestr('styles.xml', '<style:footer/>')
            errors: list[str] = []
            check_documentation.check_running_manual_footer(errors, path)
            self.assertEqual(1, len(errors))


if __name__ == '__main__':
    unittest.main()
