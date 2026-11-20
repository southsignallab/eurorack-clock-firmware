"""Every factory default is defined once in src/defaults.h and applied by the factory initialization.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULTS = ROOT / "src" / "defaults.h"
FACTORY = ROOT / "src" / "domain" / "default_configuration.cpp"
CONSTANT_RE = re.compile(r"^inline constexpr [\w:<>, ]+? (k\w+)\s*[{=]", re.MULTILINE)


class FactoryDefaultTests(unittest.TestCase):
    def test_every_default_constant_is_used_by_the_factory_initialization(self) -> None:
        # F-086: a struct default that duplicates a defaults.h constant hides which value is the factory value.
        names = CONSTANT_RE.findall(DEFAULTS.read_text(encoding="utf-8"))
        self.assertGreater(len(names), 40)
        factory = FACTORY.read_text(encoding="utf-8")
        unused = [name for name in names if not re.search(rf"\bdefaults::{name}\b", factory)]
        self.assertEqual(unused, [], "defaults.h constants not applied by initializeFactoryDefaults()")


if __name__ == "__main__":
    unittest.main()
