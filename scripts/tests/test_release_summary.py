"""Regression tests for the release-summary contract, including the Known issues section.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PYTHON = sys.executable


def load_module(name: str, path: Path):
    """Load a repository script as a testable Python module."""
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


SUMMARY = load_module("check_release_summary", ROOT / "scripts/check_release_summary.py")

INTRO = (
    "# CLOCK {version}\n\n"
    "CLOCK {version} is an eight-output Eurorack clock, divider, Euclidean rhythm source and gate sequencer. "
    "This summary text is long enough to explain a release to a user who reads the release page and wants "
    "to know what changed, what stays compatible and what is still open before they update their module.\n\n"
)


def summary_text(version: str, highlights: int = 3, known: list[str] | None = None, known_body: str = "") -> str:
    """Build a release summary with the requested number of bullets."""
    text = INTRO.format(version=version) + "## Highlights\n\n"
    text += "".join(f"- Highlight number {index}.\n" for index in range(highlights))
    if known is not None:
        text += "\n## Known issues\n\n" + known_body + "".join(f"- {item}\n" for item in known)
    text += "\n## Compatibility\n\nPresets migrate.\n"
    return text


class ReleaseSummaryKnownIssuesTests(unittest.TestCase):
    """The Known issues section has its own limit and does not count against the highlights."""

    def check(self, text: str, version: str) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "RELEASE_SUMMARY.md"
            path.write_text(text, encoding="utf-8")
            SUMMARY.validate(path, version)

    def test_known_issues_with_up_to_eight_items_pass(self) -> None:
        for count in (0, 1, 8):
            with self.subTest(count=count):
                body = "None known.\n" if count == 0 else ""
                self.check(summary_text("1.2.0", known=[f"issue {i}" for i in range(count)], known_body=body), "1.2.0")

    def test_more_than_eight_known_issues_fail(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "Known issues.*0-8"):
            self.check(summary_text("1.2.0", known=[f"issue {i}" for i in range(9)]), "1.2.0")

    def test_known_issues_do_not_count_against_the_highlight_limit(self) -> None:
        self.check(summary_text("1.2.0", highlights=10, known=["one", "two", "three"]), "1.2.0")
        with self.assertRaisesRegex(RuntimeError, "2-10"):
            self.check(summary_text("1.2.0", highlights=11, known=["one"]), "1.2.0")

    def test_known_issues_section_is_required_from_1_2_0(self) -> None:
        for version in ("1.2.0", "1.2.1-rc.1", "2.0.0"):
            with self.subTest(version=version):
                with self.assertRaisesRegex(RuntimeError, "Known issues"):
                    self.check(summary_text(version), version)
        self.check(summary_text("1.1.0"), "1.1.0")

    def test_empty_known_issues_section_must_say_none_are_known(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "none are known"):
            self.check(summary_text("1.2.0", known=[]), "1.2.0")

    def test_scaffold_contains_the_known_issues_section(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "scaffold.md"
            result = subprocess.run(
                [PYTHON, str(ROOT / "scripts/prepare_release_summary.py"), "--version", "1.2.1", "--output", str(output)],
                cwd=ROOT, text=True, capture_output=True, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            text = output.read_text(encoding="utf-8")
            self.assertIn("\n## Known issues\n", text)
            self.assertLess(text.index("## Highlights"), text.index("## Known issues"))

    def test_repository_summaries_pass(self) -> None:
        for path in sorted((ROOT / "docs" / "releases").glob("*/RELEASE_SUMMARY.md")):
            with self.subTest(version=path.parent.name):
                SUMMARY.validate(path, path.parent.name)


if __name__ == "__main__":
    unittest.main()
