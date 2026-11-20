#!/usr/bin/env python3
"""Prove that a formatting- or comment-only change leaves the firmware images unchanged.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0

Every firmware environment is built into its own build directory. For each environment the
SHA-256 of ``firmware.bin`` and of every loaded ELF section (allocated and not ``NOBITS``) is
recorded in a JSON manifest. ``record`` writes the manifest at the reference commit;
``compare`` builds the working tree again and reports equal or unequal per environment.

Only ``firmware.bin`` decides. The section hashes help to find the cause of a difference.
Debug information and the linker map are not compared: they depend on line numbers and on
the build directory.

Usage (repository root):
  python scripts/check_image_identity.py record --manifest build/image-identity/reference.json
  python scripts/check_image_identity.py compare --manifest build/image-identity/reference.json
  python scripts/check_image_identity.py compare --manifest ref.json --against other.json
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST_FORMAT = 1

# Five target builds and five release flavors (D-16 point 4).
DEFAULT_ENVIRONMENTS = (
    "blackpill_f401cc",
    "blackpill_f401cc_spi_ssd1315",
    "blackpill_f401cc_spi",
    "blackpill_f401cc_debug_instrumented",
    "blackpill_f401cc_timing",
    "release_default",
    "release_pixel_raid",
    "release_formula_1",
    "release_breakout",
    "release_egg_journey",
)

ELF_MAGIC = b"\x7fELF"
ELF_CLASS_32 = 1
ELF_CLASS_64 = 2
ELF_DATA_LSB = 1
ELF_DATA_MSB = 2
SHT_NOBITS = 8
SHF_ALLOC = 0x2


@dataclass(frozen=True)
class Section:
    """One ELF section header with the fields the identity check needs."""

    name: str
    section_type: int
    flags: int
    offset: int
    size: int

    @property
    def loaded(self) -> bool:
        """Return True for sections whose bytes end up in the programmed image."""
        return bool(self.flags & SHF_ALLOC) and self.section_type != SHT_NOBITS and self.size > 0


def parse_elf_sections(data: bytes) -> list[Section]:
    """Parse the section header table of a 32- or 64-bit ELF file.

    Raises ValueError for anything that is not a well-formed ELF file.
    """
    if len(data) < 16 or data[:4] != ELF_MAGIC:
        raise ValueError("not an ELF file")
    elf_class, elf_data = data[4], data[5]
    if elf_data == ELF_DATA_LSB:
        order = "<"
    elif elf_data == ELF_DATA_MSB:
        order = ">"
    else:
        raise ValueError(f"unknown ELF byte order {elf_data}")
    if elf_class == ELF_CLASS_32:
        header_fmt, entry_fmt = order + "16sHHIIIIIHHHHHH", order + "IIIIIIIIII"
    elif elf_class == ELF_CLASS_64:
        header_fmt, entry_fmt = order + "16sHHIQQQIHHHHHH", order + "IIQQQQIIQQ"
    else:
        raise ValueError(f"unknown ELF class {elf_class}")
    header_size = struct.calcsize(header_fmt)
    if len(data) < header_size:
        raise ValueError("truncated ELF header")
    header = struct.unpack_from(header_fmt, data, 0)
    shoff, shentsize, shnum, shstrndx = header[6], header[11], header[12], header[13]
    if shnum == 0:
        return []
    if shentsize < struct.calcsize(entry_fmt):
        raise ValueError("section header entries too small")
    if shoff + shnum * shentsize > len(data):
        raise ValueError("truncated section header table")
    if shstrndx >= shnum:
        raise ValueError("section name table index out of range")
    raw = []
    for index in range(shnum):
        fields = struct.unpack_from(entry_fmt, data, shoff + index * shentsize)
        # name, type, flags, addr, offset, size (same order in both classes)
        raw.append((fields[0], fields[1], fields[2], fields[4], fields[5]))
    _, _, _, str_offset, str_size = raw[shstrndx]
    if str_offset + str_size > len(data):
        raise ValueError("truncated section name table")
    names = data[str_offset:str_offset + str_size]
    sections = []
    for name_offset, section_type, flags, offset, size in raw:
        if name_offset >= len(names) and name_offset != 0:
            raise ValueError("section name offset out of range")
        end = names.find(b"\0", name_offset)
        name = names[name_offset:end if end >= 0 else len(names)].decode("ascii", "replace")
        if section_type != SHT_NOBITS and offset + size > len(data):
            raise ValueError(f"section {name} exceeds the file")
        sections.append(Section(name, section_type, flags, offset, size))
    return sections


def loaded_section_hashes(data: bytes) -> dict[str, str]:
    """Return SHA-256 per loaded section; duplicate names get an index suffix."""
    hashes: dict[str, str] = {}
    for section in parse_elf_sections(data):
        if not section.loaded:
            continue
        key = section.name
        suffix = 1
        while key in hashes:
            suffix += 1
            key = f"{section.name}#{suffix}"
        hashes[key] = hashlib.sha256(data[section.offset:section.offset + section.size]).hexdigest()
    return hashes


def image_record(bin_path: Path, elf_path: Path) -> dict:
    """Return the manifest entry for one environment."""
    binary = bin_path.read_bytes()
    return {
        "firmware_bin_sha256": hashlib.sha256(binary).hexdigest(),
        "firmware_bin_bytes": len(binary),
        "sections": loaded_section_hashes(elf_path.read_bytes()),
    }


@dataclass(frozen=True)
class EnvironmentResult:
    """Comparison outcome for one environment."""

    environment: str
    equal: bool
    detail: str


def compare_manifests(reference: dict, current: dict, environments: list[str]) -> list[EnvironmentResult]:
    """Compare two manifests; only firmware.bin decides, sections explain."""
    results = []
    ref_envs, cur_envs = reference.get("environments", {}), current.get("environments", {})
    for env in environments:
        ref, cur = ref_envs.get(env), cur_envs.get(env)
        if ref is None or cur is None:
            missing = "reference" if ref is None else "current build"
            results.append(EnvironmentResult(env, False, f"missing in {missing}"))
            continue
        if ref["firmware_bin_sha256"] == cur["firmware_bin_sha256"]:
            results.append(EnvironmentResult(env, True, ref["firmware_bin_sha256"][:16]))
            continue
        ref_sec, cur_sec = ref.get("sections", {}), cur.get("sections", {})
        changed = sorted(n for n in ref_sec.keys() & cur_sec.keys() if ref_sec[n] != cur_sec[n])
        only_ref = sorted(ref_sec.keys() - cur_sec.keys())
        only_cur = sorted(cur_sec.keys() - ref_sec.keys())
        parts = [f"bin {ref['firmware_bin_bytes']} -> {cur['firmware_bin_bytes']} B"]
        if changed:
            parts.append("changed sections: " + ", ".join(changed))
        if only_ref:
            parts.append("removed sections: " + ", ".join(only_ref))
        if only_cur:
            parts.append("new sections: " + ", ".join(only_cur))
        results.append(EnvironmentResult(env, False, "; ".join(parts)))
    return results


def git_state(root: Path) -> dict:
    """Return HEAD and whether the working tree differs from it (None outside Git)."""
    def run(*args: str) -> str | None:
        result = subprocess.run(["git", *args], cwd=root, capture_output=True, text=True,
                                encoding="utf-8", check=False)
        return result.stdout if result.returncode == 0 else None

    head = run("rev-parse", "HEAD")
    status = run("status", "--short")
    return {"head": head.strip() if head else None,
            "dirty": None if status is None else bool(status.strip())}


def build_environment(pio: str, env: str, build_root: Path, root: Path, log_dir: Path,
                      attempts: int = 2) -> tuple[Path, Path]:
    """Build one environment into its own directory and return (bin, elf).

    A failed build is repeated from a clean directory up to ``attempts`` times in total:
    on Windows, a scanner can briefly lock a fresh archive (``ranlib: Permission denied``).
    Every attempt keeps its own log; a real compile error fails every attempt.
    """
    build_dir = build_root / env
    log_dir.mkdir(parents=True, exist_ok=True)
    environ = dict(os.environ, PLATFORMIO_BUILD_DIR=str(build_dir))
    for attempt in range(1, attempts + 1):
        if build_dir.exists():
            shutil.rmtree(build_dir)
        log_path = log_dir / (f"{env}.log" if attempt == 1 else f"{env}.attempt{attempt}.log")
        with log_path.open("w", encoding="utf-8") as log:
            result = subprocess.run([pio, "run", "-e", env], cwd=root, env=environ, stdout=log,
                                    stderr=subprocess.STDOUT, check=False)
        if result.returncode == 0:
            break
        print(f"{env}: build attempt {attempt} failed (exit {result.returncode}), see {log_path}", flush=True)
    else:
        raise RuntimeError(f"{env}: build failed {attempts} times, see {log_dir}")
    out = build_dir / env
    return out / "firmware.bin", out / "firmware.elf"


def build_manifest(environments: list[str], build_root: Path, pio: str, root: Path) -> dict:
    """Build every environment and return the manifest."""
    manifest = {"format": MANIFEST_FORMAT, "git": git_state(root), "environments": {}}
    for env in environments:
        bin_path, elf_path = build_environment(pio, env, build_root, root, build_root / "logs")
        manifest["environments"][env] = image_record(bin_path, elf_path)
        print(f"built {env}: {manifest['environments'][env]['firmware_bin_sha256'][:16]}", flush=True)
    return manifest


def load_manifest(path: Path) -> dict:
    """Read a manifest and check its format."""
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if manifest.get("format") != MANIFEST_FORMAT:
        raise SystemExit(f"{path}: unsupported manifest format {manifest.get('format')!r}")
    return manifest


def write_manifest(path: Path, manifest: dict) -> None:
    """Write a manifest with LF line endings."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes((json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8"))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("mode", choices=("record", "compare"))
    parser.add_argument("--manifest", type=Path, required=True,
                        help="reference manifest (written by record, read by compare)")
    parser.add_argument("--against", type=Path,
                        help="compare: use this manifest instead of building the working tree")
    parser.add_argument("--output", type=Path,
                        help="compare: also write the manifest of the current build here")
    parser.add_argument("--env", action="append", dest="environments",
                        help="environment to check (repeatable; default: all ten)")
    parser.add_argument("--build-root", type=Path, default=ROOT / "build" / "image-identity",
                        help="parent of the per-environment build directories")
    parser.add_argument("--pio", default=shutil.which("pio") or "pio", help="PlatformIO executable")
    args = parser.parse_args(argv)
    environments = args.environments or list(DEFAULT_ENVIRONMENTS)

    if args.mode == "record":
        manifest = build_manifest(environments, args.build_root / "reference", args.pio, ROOT)
        write_manifest(args.manifest, manifest)
        print(f"reference manifest {args.manifest}: {len(environments)} environments, "
              f"HEAD {manifest['git']['head']}, dirty {manifest['git']['dirty']}")
        return 0

    reference = load_manifest(args.manifest)
    if args.against:
        current = load_manifest(args.against)
    else:
        current = build_manifest(environments, args.build_root / "current", args.pio, ROOT)
        if args.output:
            write_manifest(args.output, current)
    print(f"reference HEAD {reference['git'].get('head')} dirty {reference['git'].get('dirty')}; "
          f"current HEAD {current['git'].get('head')} dirty {current['git'].get('dirty')}")
    results = compare_manifests(reference, current, environments)
    for result in results:
        print(f"{result.environment}: {'equal' if result.equal else 'UNEQUAL'} ({result.detail})")
    unequal = sum(not r.equal for r in results)
    print(f"image identity: {len(results) - unequal} equal, {unequal} unequal")
    return 0 if unequal == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
