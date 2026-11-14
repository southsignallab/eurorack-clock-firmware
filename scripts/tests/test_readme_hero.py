#!/usr/bin/env python3
"""Regression tests for the generated README hero images.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""

from __future__ import annotations

import importlib.util
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/generate_readme_hero.py'
spec = importlib.util.spec_from_file_location('generate_readme_hero', SCRIPT)
assert spec is not None and spec.loader is not None
hero = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = hero
spec.loader.exec_module(hero)


class ReadmeHeroTests(unittest.TestCase):
    """Keep the README hero tied to the firmware version and its committed inputs."""

    def test_status_line_distinguishes_release_and_pre_release(self) -> None:
        self.assertEqual('1.1.0 · released', hero.status_text('1.1.0'))
        self.assertEqual('1.2.0-rc.1 · pre-release', hero.status_text('1.2.0-rc.1'))
        self.assertEqual('1.2.0-alpha · pre-release', hero.status_text('1.2.0-alpha'))

    def test_version_comes_from_firmware_header(self) -> None:
        result = subprocess.run([sys.executable, str(ROOT / 'scripts/version.py')], capture_output=True, text=True, check=True, cwd=ROOT)
        self.assertEqual(result.stdout.strip(), hero.firmware_version())

    def test_committed_images_pass_check(self) -> None:
        result = subprocess.run([sys.executable, str(SCRIPT), '--check'], capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stderr)

    def test_check_fails_on_version_mismatch(self) -> None:
        errors = hero.check(hero.OUTPUT_DIR, '9.9.9')
        self.assertEqual(2, len(errors))
        self.assertTrue(all('version mismatch' in error for error in errors), errors)

    def test_check_fails_on_missing_image(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            shutil.copy(hero.OUTPUT_DIR / 'clock-firmware-hero-light.webp', tmp)
            errors = hero.check(Path(tmp), hero.firmware_version())
        self.assertEqual(1, len(errors))
        self.assertIn('clock-firmware-hero-dark.webp', errors[0])

    def test_output_size_is_twice_the_css_layout(self) -> None:
        image = hero.render(hero.THEMES[0], '1.1.0')
        self.assertEqual((2560, 1200), image.size)

    def test_readme_references_both_hero_variants(self) -> None:
        readme = (ROOT / 'README.md').read_text(encoding='utf-8')
        self.assertIn('docs/assets/readme/clock-firmware-hero-light.webp', readme)
        self.assertIn('docs/assets/readme/clock-firmware-hero-dark.webp', readme)


if __name__ == '__main__':
    unittest.main()
