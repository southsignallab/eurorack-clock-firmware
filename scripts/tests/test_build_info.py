"""Regression tests for the path-independent build provenance in package_release.py.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import runpy
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
MODULE = runpy.run_path(str(ROOT / "scripts" / "package_release.py"))
normalized_map_text = MODULE["normalized_map_text"]


def map_text(build: str, packages: str) -> str:
    """Build a small GNU ld map excerpt that names the build directory and the toolchain."""
    return (
        "Archive member included to satisfy reference by file (symbol)\n\n"
        f"{build}/lib6a9/libclock_core.a(clock_core.o)\n"
        f"                              {build}/src/engine/clock_engine_timing.o (symbol)\n"
        f"{packages}/toolchain-gccarmnoneeabi@1.70201.0/bin/../arm-none-eabi/lib/thumb/v7e-m\\libc_nano.a\n"
        f"LOAD {build.replace('/', chr(92))}\\src\\main.o\n"
    )


class BuildInfoNormalizationTests(unittest.TestCase):
    def test_two_build_directories_give_the_same_normalized_map(self) -> None:
        packages = "c:/users/builder/.platformio/packages"
        first = normalized_map_text(map_text(".pio/build/release_default", packages),
                                    Path(".pio/build/release_default"), Path(packages))
        second = normalized_map_text(map_text("build/other/release_default", packages),
                                     Path("build/other/release_default"), Path(packages))
        self.assertEqual(first, second)
        self.assertIn("<build>/src/engine/clock_engine_timing.o", first)
        self.assertIn("<build>\\src\\main.o", first)
        self.assertIn("<packages>/toolchain-gccarmnoneeabi@1.70201.0", first)

    def test_absolute_build_directory_and_case_are_normalized(self) -> None:
        packages = "C:/Users/Builder/.platformio/packages"
        absolute = "C:/work/clock/.pio/build/release_default"
        text = normalized_map_text(map_text(absolute.lower(), packages.lower()), Path(absolute), Path(packages))
        self.assertNotIn("c:/work", text)
        self.assertNotIn("users/builder", text)

    def test_archives_are_listed_with_normalized_paths(self) -> None:
        packages = "c:/users/builder/.platformio/packages"
        text = normalized_map_text(map_text("build/x/release_default", packages),
                                   Path("build/x/release_default"), Path(packages))
        archives = MODULE["linked_archives"](text)
        self.assertIn("<build>/lib6a9/libclock_core.a", archives)


if __name__ == "__main__":
    unittest.main()
