#!/usr/bin/env python3
"""Configure CLOCK's host simulator with the documented Windows UCRT64 GCC.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PRESETS = ("simulator", "simulator-headless", "simulator-headless-release")


def compiler_paths(environ: dict[str, str]) -> tuple[Path, Path]:
    """Resolve Windows UCRT64 compilers without choosing an unrelated cl.exe."""
    msystem_prefix = environ.get("MSYSTEM_PREFIX", "")
    prefix = (
        environ.get("CLOCK_UCRT64_ROOT")
        or (msystem_prefix if Path(msystem_prefix).name.lower() == "ucrt64" else "")
        or r"C:\msys64\ucrt64"
    )
    bin_dir = Path(prefix) / "bin"
    gcc, gxx = bin_dir / "gcc.exe", bin_dir / "g++.exe"
    if not gcc.is_file() or not gxx.is_file():
        raise RuntimeError(
            "CLOCK requires MSYS2 UCRT64 gcc.exe and g++.exe for native Windows builds. "
            f"Not found under {bin_dir}. Set CLOCK_UCRT64_ROOT to the installation root."
        )
    return gcc, gxx


def cached_value(cache: Path, name: str) -> str | None:
    """Read one CMake cache entry, if a build directory has been configured."""
    if not cache.exists():
        return None
    content = cache.read_text(encoding="utf-8", errors="replace")
    result = re.search(rf"^{re.escape(name)}:[^=]+=([^\r\n]*)$", content, flags=re.MULTILINE)
    return result.group(1) if result else None


def stale_windows_cache(cache: Path, gcc: Path, gxx: Path) -> bool:
    """A CMake cache never safely switches generators/compilers in place."""
    for key, required in (("CMAKE_C_COMPILER", gcc), ("CMAKE_CXX_COMPILER", gxx)):
        found = cached_value(cache, key)
        if found and Path(found).resolve() != required.resolve():
            return True
    generator = cached_value(cache, "CMAKE_GENERATOR")
    return generator is not None and generator != "Ninja"


def validate_compiler(gxx: Path, environment: dict[str, str]) -> None:
    """Detect broken UCRT64 installations and missing C++ headers early."""
    with tempfile.TemporaryDirectory(prefix="clock-ucrt64-") as directory:
        source = Path(directory) / "verify.cpp"
        executable = Path(directory) / "verify.exe"
        source.write_text(
            "#include <array>\n#include <cstdint>\n"
            "static_assert(sizeof(std::uint32_t) == 4);\n"
            "std::array<int, 2> values{1, 2};\n"
            "int main() { return values[0] == 1 ? 0 : 1; }\n",
            encoding="utf-8",
        )
        subprocess.run(
            [str(gxx), "-std=c++17", str(source), "-o", str(executable)],
            check=True,
            env=environment,
        )
        subprocess.run([str(executable)], check=True, env=environment)


def run(preset: str, build: bool, *, platform: str = sys.platform) -> None:
    environment = os.environ.copy()
    binary_dir = ROOT / "build" / preset
    if platform == "win32":
        gcc, gxx = compiler_paths(environment)
        environment["PATH"] = str(gcc.parent) + os.pathsep + environment.get("PATH", "")
        validate_compiler(gxx, environment)
        cache = binary_dir / "CMakeCache.txt"
        if stale_windows_cache(cache, gcc, gxx):
            print(f"Removing stale CMake toolchain cache: {binary_dir}", flush=True)
            shutil.rmtree(binary_dir)
        environment["CC"] = str(gcc)
        environment["CXX"] = str(gxx)
        print(f"CLOCK Windows host compiler: {gxx}", flush=True)

    subprocess.run(["cmake", "--preset", preset], check=True, cwd=ROOT, env=environment)
    if build:
        subprocess.run(["cmake", "--build", "--preset", preset], check=True, cwd=ROOT, env=environment)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preset", choices=PRESETS, default="simulator")
    parser.add_argument("--build", action="store_true")
    args = parser.parse_args()
    try:
        run(args.preset, args.build)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"CLOCK simulator configuration failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
