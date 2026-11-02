"""Regression coverage for Windows simulator host-toolchain selection.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("configure_simulator", ROOT / "scripts" / "configure_simulator.py")
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class SimulatorToolchainTest(unittest.TestCase):
    def test_find_ucrt64_compiler_even_if_mingw64_shell_active(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "bin").mkdir()
            (root / "bin" / "gcc.exe").touch()
            (root / "bin" / "g++.exe").touch()
            cc, cxx = module.compiler_paths({
                "CLOCK_UCRT64_ROOT": directory,
                "MSYSTEM_PREFIX": r"C:\msys64\mingw64",
            })
            self.assertEqual(cc, root / "bin" / "gcc.exe")
            self.assertEqual(cxx, root / "bin" / "g++.exe")

    def test_missing_toolchain_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, "MSYS2 UCRT64"):
                module.compiler_paths({"CLOCK_UCRT64_ROOT": directory})

    def test_recognizes_old_msvc_compiler_and_ninja_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_text(
                "CMAKE_CXX_COMPILER:FILEPATH=C:/Program Files (x86)/Microsoft Visual Studio 12.0/VC/bin/amd64/cl.exe\n"
                "CMAKE_C_COMPILER:FILEPATH=C:/Program Files (x86)/Microsoft Visual Studio 12.0/VC/bin/amd64/cl.exe\n"
                "CMAKE_GENERATOR:INTERNAL=Ninja\n",
                encoding="utf-8",
            )
            self.assertTrue(module.stale_windows_cache(cache, Path("gcc.exe"), Path("g++.exe")))
            self.assertIn("Visual Studio 12.0", module.cached_value(cache, "CMAKE_CXX_COMPILER"))
            cache.write_text("CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022\n", encoding="utf-8")
            self.assertTrue(module.stale_windows_cache(cache, Path("gcc.exe"), Path("g++.exe")))

    def test_matching_compilers_reuse_cache(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            gcc = root / "gcc.exe"
            gxx = root / "g++.exe"
            cache = root / "CMakeCache.txt"
            cache.write_text(
                f"CMAKE_C_COMPILER:FILEPATH={gcc.as_posix()}\n"
                f"CMAKE_CXX_COMPILER:FILEPATH={gxx.as_posix()}\n"
                "CMAKE_GENERATOR:INTERNAL=Ninja\n",
                encoding="utf-8",
            )
            self.assertFalse(module.stale_windows_cache(cache, gcc, gxx))

    def test_windows_build_repairs_old_cache_and_uses_ucrt64(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            toolchain_bin = root / "bin"
            toolchain_bin.mkdir()
            gcc = toolchain_bin / "gcc.exe"
            gxx = toolchain_bin / "g++.exe"
            gcc.touch()
            gxx.touch()
            build_dir = root / "build" / "simulator"
            build_dir.mkdir(parents=True)
            old_marker = build_dir / "old-state.txt"
            old_marker.touch()
            (build_dir / "CMakeCache.txt").write_text(
                "CMAKE_CXX_COMPILER:FILEPATH=C:/Program Files (x86)/Microsoft Visual Studio 12.0/VC/bin/amd64/cl.exe\n"
                "CMAKE_GENERATOR:INTERNAL=Ninja\n", encoding="utf-8"
            )
            with mock.patch.object(module, "ROOT", root), mock.patch.dict(
                module.os.environ, {"CLOCK_UCRT64_ROOT": str(root)}
            ), mock.patch.object(module, "validate_compiler"), mock.patch.object(
                module.subprocess, "run"
            ) as called:
                module.run("simulator", True, platform="win32")
            self.assertFalse(old_marker.exists())
            self.assertEqual(called.call_count, 2)
            self.assertEqual(called.call_args_list[0].args[0], ["cmake", "--preset", "simulator"])
            self.assertEqual(called.call_args_list[1].args[0], ["cmake", "--build", "--preset", "simulator"])
            self.assertEqual(called.call_args_list[0].kwargs["env"]["CXX"], str(gxx))
            self.assertEqual(called.call_args_list[1].kwargs["env"]["CC"], str(gcc))
            self.assertTrue(called.call_args_list[0].kwargs["env"]["PATH"].startswith(str(toolchain_bin)))

    def test_windows_reconfig_rejects_broken_gcc_before_clearing_build(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "bin").mkdir()
            (root / "bin" / "gcc.exe").touch()
            (root / "bin" / "g++.exe").touch()
            with mock.patch.object(module, "ROOT", root), mock.patch.dict(
                module.os.environ, {"CLOCK_UCRT64_ROOT": str(root)}
            ), mock.patch.object(module, "validate_compiler", side_effect=RuntimeError("broken GCC")), mock.patch.object(
                module.shutil, "rmtree"
            ) as remove:
                with self.assertRaisesRegex(RuntimeError, "broken GCC"):
                    module.run("simulator", False, platform="win32")
                remove.assert_not_called()


if __name__ == "__main__":
    unittest.main()
