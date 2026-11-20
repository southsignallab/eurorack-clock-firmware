#!/usr/bin/env python3
"""Regression tests for anchors in documentation links and the User Guide index.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""

from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/check_documentation.py'
spec = importlib.util.spec_from_file_location('check_documentation_links', SCRIPT)
assert spec is not None and spec.loader is not None
check_documentation = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = check_documentation
spec.loader.exec_module(check_documentation)


class GithubSlugTests(unittest.TestCase):
    """Heading anchors follow the GitHub rules, checked on examples from this repository."""

    def test_examples(self) -> None:
        cases = {
            '21. Hidden boot Easter eggs': '21-hidden-boot-easter-eggs',
            'Swing, Groove, and Humanize — what changes?': 'swing-groove-and-humanize--what-changes',
            'Master meter: `METER BEATS` and `METER UNIT`': 'master-meter-meter-beats-and-meter-unit',
            'SOURCE — who owns musical time?': 'source--who-owns-musical-time',
            'PPQN, EDGE, FILTER, SMOOTHING, TIMEOUT and AUTOSTART': 'ppqn-edge-filter-smoothing-timeout-and-autostart',
            '15B. Windows workstation': '15b-windows-workstation',
            'snake_case and **bold** [link](x.md)': 'snake_case-and-bold-link',
            'Ünïcode Größe': 'ünïcode-größe',
        }
        for heading, slug in cases.items():
            with self.subTest(heading=heading):
                self.assertEqual(slug, check_documentation.github_slug(heading))

    def test_duplicate_headings_get_numbered_suffixes(self) -> None:
        text = '# Notes\n\n## Notes\n\n```\n# not a heading\n```\n\n### Notes\n'
        self.assertEqual({'notes', 'notes-1', 'notes-2'}, check_documentation.markdown_anchors(text))

    def test_explicit_html_anchors_count(self) -> None:
        text = '<a id="top"></a>\n<a name="legacy"></a>\n# Title\n'
        self.assertEqual({'top', 'legacy', 'title'}, check_documentation.markdown_anchors(text))


class AnchorLinkTests(unittest.TestCase):
    """A link to a missing heading fails the documentation check."""

    def write(self, root: Path, name: str, text: str) -> Path:
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        return path

    def run_check(self, root: Path, paths: list[Path]) -> list[str]:
        errors: list[str] = []
        check_documentation.check_links(errors, paths, root)
        return errors

    def test_broken_anchor_in_other_file_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write(root, 'b.md', '# Bee\n\n## Real section\n')
            a = self.write(root, 'a.md', '[ok](b.md#real-section) [bad](b.md#missing-section)\n')
            errors = self.run_check(root, [a])
            self.assertEqual(1, len(errors))
            self.assertIn('missing anchor', errors[0])
            self.assertIn('b.md#missing-section', errors[0])

    def test_broken_in_page_anchor_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = self.write(root, 'a.md', '# Title\n\nSee [here](#title) and [there](#21-hidden-boot-easter-eggs).\n')
            errors = self.run_check(root, [a])
            self.assertEqual(1, len(errors))
            self.assertIn('#21-hidden-boot-easter-eggs', errors[0])

    def test_html_link_anchor_is_checked(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write(root, 'b.md', '# Bee\n')
            a = self.write(root, 'a.md', '<a href="b.md#bee">ok</a> <a href="b.md#wasp">bad</a>\n')
            self.assertEqual(1, len(self.run_check(root, [a])))

    def test_anchor_on_non_markdown_target_is_not_checked(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.write(root, 'page.html', '<html></html>')
            a = self.write(root, 'a.md', '[x](page.html#anything)\n')
            self.assertEqual([], self.run_check(root, [a]))

    def test_anchors_inside_code_fences_are_not_checked_but_files_are(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = self.write(root, 'a.md', '# T\n\n```\n[x](#nowhere) [y](missing.md)\n```\n')
            errors = self.run_check(root, [a])
            self.assertEqual(1, len(errors))
            self.assertIn('missing local link target: missing.md', errors[0])

    def test_vcv_documents_are_scanned(self) -> None:
        names = {path.relative_to(ROOT).as_posix() for path in check_documentation.markdown_files()}
        self.assertIn('vcv/README.md', names)

    def test_repository_has_no_broken_anchor(self) -> None:
        errors: list[str] = []
        check_documentation.check_links(errors)
        self.assertEqual([], [e for e in errors if 'anchor' in e])


class DocumentationMapTests(unittest.TestCase):
    """Every Markdown document has one inventory row; every row names an existing file."""

    MAP = ('# Map\n\n## Inventory\n\n| Path | Purpose | Audience |\n| --- | --- | --- |\n'
           '| `README.md` | Landing page | all |\n| `docs/user-guide/[0-9][0-9]-*.md` | Chapter | end user |\n')

    def setup_tree(self, root: Path, extra_rows: str = '') -> Path:
        for name in ('README.md', 'docs/user-guide/01-start.md', 'docs/user-guide/02-panel.md'):
            (root / name).parent.mkdir(parents=True, exist_ok=True)
            (root / name).write_text('# x\n', encoding='utf-8')
        map_path = root / 'docs' / 'DOCUMENTATION_MAP.md'
        map_path.write_text(self.MAP + extra_rows, encoding='utf-8')
        return map_path

    def test_complete_map_passes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            map_path = self.setup_tree(root)
            errors: list[str] = []
            check_documentation.check_documentation_map(
                errors, map_path, [root / 'README.md', root / 'docs/user-guide/01-start.md'], root)
            self.assertEqual([], errors)

    def test_unlisted_document_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            map_path = self.setup_tree(root)
            (root / 'docs' / 'NEW.md').write_text('# New\n', encoding='utf-8')
            errors: list[str] = []
            check_documentation.check_documentation_map(errors, map_path, [root / 'README.md', root / 'docs/NEW.md'], root)
            self.assertEqual(1, len(errors))
            self.assertIn('docs/NEW.md', errors[0])

    def test_row_without_file_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            map_path = self.setup_tree(root, '| `docs/GONE.md` | Removed | all |\n')
            errors: list[str] = []
            check_documentation.check_documentation_map(errors, map_path, [root / 'README.md'], root)
            self.assertEqual(1, len(errors))
            self.assertIn('docs/GONE.md', errors[0])

    def test_repository_map_is_complete(self) -> None:
        errors: list[str] = []
        check_documentation.check_documentation_map(errors)
        self.assertEqual([], errors)


class UserGuideIndexTests(unittest.TestCase):
    """docs/USER_GUIDE.md is an index of the chapter files, not a second copy."""

    def test_repository_index_passes(self) -> None:
        errors: list[str] = []
        check_documentation.check_user_guide_index(errors)
        self.assertEqual([], errors)

    def test_index_with_operating_section_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            index = Path(tmp) / 'USER_GUIDE.md'
            chapters = sorted((ROOT / 'docs/user-guide').glob('[0-9][0-9]-*.md'))
            links = '\n'.join(f'- [{p.stem}](user-guide/{p.name})' for p in chapters)
            index.write_text(f'# Guide\n\n## Chapters\n\n{links}\n\n## 2. Quick start\n\nPress START.\n',
                             encoding='utf-8')
            errors: list[str] = []
            check_documentation.check_user_guide_index(errors, index)
            self.assertEqual(1, len(errors))
            self.assertIn('2. Quick start', errors[0])

    def test_index_missing_a_chapter_link_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            index = Path(tmp) / 'USER_GUIDE.md'
            index.write_text('# Guide\n\n## Chapters\n\n- [1](user-guide/01-start-here.md)\n', encoding='utf-8')
            errors: list[str] = []
            check_documentation.check_user_guide_index(errors, index)
            self.assertEqual(23, len(errors))


if __name__ == '__main__':
    unittest.main()
