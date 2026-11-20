"""Regression tests for documentation tables generated from the firmware sources.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import importlib.util
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('generate_reference_tables', ROOT / 'scripts' / 'generate_reference_tables.py')
assert spec is not None and spec.loader is not None
tables = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = tables
spec.loader.exec_module(tables)


def copy_tree(target: Path) -> Path:
    """Copy the files the generator reads into a scratch repository."""
    for relative in tables.SOURCES + tables.DOCUMENTS:
        (target / relative).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / relative, target / relative)
    return target


class ReferenceTableTests(unittest.TestCase):
    def test_repository_tables_are_current(self) -> None:
        self.assertEqual([], tables.run(check=True))

    def test_pin_change_in_code_makes_the_check_fail(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = copy_tree(Path(tmp))
            pin_map = root / 'src' / 'pin_map.h'
            text = pin_map.read_text(encoding='utf-8')
            self.assertIn('kTapTempoButtonPin = mcu::PA3;', text)
            pin_map.write_text(text.replace('kTapTempoButtonPin = mcu::PA3;', 'kTapTempoButtonPin = mcu::PA10;'),
                               encoding='utf-8')
            self.assertEqual(['docs/CONFIGURATION.md: pins'], tables.run(check=True, root=root))
            tables.run(check=False, root=root)
            self.assertIn('| TAP TEMPO button | PA10 |', (root / 'docs' / 'CONFIGURATION.md').read_text(encoding='utf-8'))
            self.assertEqual([], tables.run(check=True, root=root))

    def test_display_macro_default_change_makes_the_check_fail(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = copy_tree(Path(tmp))
            config = root / 'src' / 'config.h'
            text = config.read_text(encoding='utf-8')
            config.write_text(text.replace('#define CLOCK_DISPLAY_SPI_FREQUENCY_HZ 1000000UL',
                                           '#define CLOCK_DISPLAY_SPI_FREQUENCY_HZ 2000000UL'), encoding='utf-8')
            self.assertEqual(['docs/CONFIGURATION.md: display-macros', 'docs/TIMING.md: timing-constants'],
                             tables.run(check=True, root=root))

    def test_new_pin_constant_without_text_stops_the_generator(self) -> None:
        pin_map = (ROOT / 'src' / 'pin_map.h').read_text(encoding='utf-8')
        extended = pin_map + '\ninline constexpr std::uint32_t kNewLampPin = mcu::PC13;\n'
        with self.assertRaisesRegex(ValueError, 'kNewLampPin'):
            tables.pin_table(extended)

    def test_unassigned_signal_is_shown_as_not_mcu_controlled(self) -> None:
        body = tables.pin_table((ROOT / 'src' / 'pin_map.h').read_text(encoding='utf-8'))
        self.assertIn('| Gate-buffer `/OE` | not MCU-controlled in the final pin map |', body)
        self.assertTrue(body.splitlines()[-1].startswith('| Gate-buffer `/OE`'))

    def test_missing_markers_are_reported(self) -> None:
        table = tables.Table('pins', Path('CONFIGURATION.md'), '| a |')
        with self.assertRaisesRegex(ValueError, 'markers'):
            tables.replace_table('no markers here', table)

    def test_check_mode_does_not_write(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = copy_tree(Path(tmp))
            doc = root / 'docs' / 'CONFIGURATION.md'
            stale = doc.read_text(encoding='utf-8').replace('| TAP TEMPO button | PA3 |', '| TAP TEMPO button | PA0 |')
            doc.write_text(stale, encoding='utf-8')
            self.assertEqual(['docs/CONFIGURATION.md: pins'], tables.run(check=True, root=root))
            self.assertEqual(stale, doc.read_text(encoding='utf-8'))

    def test_crlf_checkout_is_current_when_the_tables_match(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = copy_tree(Path(tmp))
            doc = root / 'docs' / 'CONFIGURATION.md'
            doc.write_bytes(doc.read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
            self.assertEqual([], tables.run(check=True, root=root))


    def replace_in(self, root: Path, relative: str, old: str, new: str) -> None:
        """Replace one exact occurrence in a copied source file."""
        path = root / relative
        text = path.read_text(encoding='utf-8')
        self.assertEqual(1, text.count(old), old)
        path.write_text(text.replace(old, new), encoding='utf-8')

    def test_persistence_region_change_in_code_makes_the_check_fail(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = copy_tree(Path(tmp))
            self.replace_in(root, 'src/hal/persistent_layout.h',
                            'kLegacyScoreRegionBytes = 4U * 16U;', 'kLegacyScoreRegionBytes = 2U * 16U;')
            self.assertEqual(['docs/ARCHITECTURE.md: persistence-map'], tables.run(check=True, root=root))
            tables.run(check=False, root=root)
            architecture = (root / 'docs' / 'ARCHITECTURE.md').read_text(encoding='utf-8')
            self.assertIn('| 3072–3103 | 32 |', architecture)
            self.assertIn('| 3104–4095 | 992 |', architecture)

    def test_overlapping_custom_groove_records_stop_the_generator(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = copy_tree(Path(tmp))
            self.replace_in(root, 'src/hal/persistent_layout.h',
                            'kLegacyScoreRegionBytes = 4U * 16U;', 'kLegacyScoreRegionBytes = 8U * 16U;')
            with self.assertRaisesRegex(ValueError, 'overlap'):
                tables.run(check=True, root=root)

    def test_persistence_map_covers_the_image_without_gaps(self) -> None:
        body = tables.persistence_map(ROOT)
        rows = [line.split('|')[1].strip() for line in body.splitlines()[2:]]
        spans = [tuple(int(part) for part in row.split('–')) for row in rows]
        self.assertEqual(0, spans[0][0])
        for (_, end), (start, _) in zip(spans, spans[1:]):
            self.assertEqual(end + 1, start)
        self.assertEqual(12287, spans[-1][1])

    def test_scheduler_rate_change_makes_the_timing_check_fail(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = copy_tree(Path(tmp))
            self.replace_in(root, 'src/config.h', 'kSchedulerFrequencyHz = 20000UL;', 'kSchedulerFrequencyHz = 10000UL;')
            self.assertEqual(['docs/TIMING.md: timing-constants'], tables.run(check=True, root=root))
            tables.run(check=False, root=root)
            self.assertIn('| Scheduler quantum | 100 µs |', (root / 'docs' / 'TIMING.md').read_text(encoding='utf-8'))

    def test_interrupt_priority_change_makes_the_timing_check_fail(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = copy_tree(Path(tmp))
            self.replace_in(root, tables.EXTI_PRIORITY_SOURCE, 'kExtiPreemptPriority = 4U;', 'kExtiPreemptPriority = 5U;')
            self.assertEqual(['docs/TIMING.md: interrupt-priorities'], tables.run(check=True, root=root))

    def test_constant_evaluator_rejects_unknown_names_and_calls(self) -> None:
        self.assertEqual(50, tables.evaluate('1000000UL / kRate', {'kRate': 20000}))
        self.assertEqual(4, tables.evaluate('static_cast<std::size_t>(kCount)', {'kCount': 4}))
        with self.assertRaisesRegex(ValueError, 'kMissing'):
            tables.evaluate('kMissing + 1U', {})
        with self.assertRaisesRegex(ValueError, 'unsupported'):
            tables.evaluate('open(1)', {})


if __name__ == '__main__':
    unittest.main()
