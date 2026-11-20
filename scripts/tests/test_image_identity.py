"""Regression tests for the firmware image-identity check.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load_module(name: str, path: Path):
    """Load a repository script as a testable Python module."""
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


identity = load_module("check_image_identity", ROOT / "scripts" / "check_image_identity.py")

SHT_PROGBITS = 1
SHT_STRTAB = 3
SHT_NOBITS = 8
SHF_WRITE = 0x1
SHF_ALLOC = 0x2
SHF_EXECINSTR = 0x4


def make_elf32(sections: list[tuple[str, int, int, bytes]], order: str = "<") -> bytes:
    """Build a minimal ELF32 file: sections are (name, type, flags, payload)."""
    names = b"\0"
    name_offsets = []
    for name, *_ in sections:
        name_offsets.append(len(names))
        names += name.encode() + b"\0"
    shstr_name = len(names)
    names += b".shstrtab\0"
    header_size = 52
    body = b""
    offsets = []
    for _, section_type, _, payload in sections:
        offsets.append(header_size + len(body))
        if section_type != SHT_NOBITS:
            body += payload
    names_offset = header_size + len(body)
    body += names
    shoff = header_size + len(body)
    entries = [struct.pack(order + "IIIIIIIIII", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)]
    for (name, section_type, flags, payload), name_offset, offset in zip(sections, name_offsets, offsets):
        entries.append(struct.pack(order + "IIIIIIIIII", name_offset, section_type, flags, 0x08000000,
                                   offset, len(payload), 0, 0, 4, 0))
    entries.append(struct.pack(order + "IIIIIIIIII", shstr_name, SHT_STRTAB, 0, 0, names_offset,
                               len(names), 0, 0, 1, 0))
    shnum = len(entries)
    ident = b"\x7fELF" + bytes([1, 1 if order == "<" else 2, 1]) + b"\0" * 9
    header = struct.pack(order + "16sHHIIIIIHHHHHH", ident, 2, 40, 1, 0x08000189, 0, shoff, 0,
                         header_size, 0, 0, 40, shnum, shnum - 1)
    return header + body + b"".join(entries)


SAMPLE_SECTIONS = [
    (".isr_vector", SHT_PROGBITS, SHF_ALLOC, b"\x01\x02\x03\x04"),
    (".text", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, b"code" * 8),
    (".data", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, b"data"),
    (".bss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE, b"\0" * 64),
    (".debug_line", SHT_PROGBITS, 0, b"lines that move with formatting"),
]


class ElfParsingTests(unittest.TestCase):
    def test_parses_section_names_types_and_sizes(self) -> None:
        sections = identity.parse_elf_sections(make_elf32(SAMPLE_SECTIONS))
        self.assertEqual([s.name for s in sections],
                         ["", ".isr_vector", ".text", ".data", ".bss", ".debug_line", ".shstrtab"])
        self.assertEqual(sections[2].size, 32)
        self.assertEqual(sections[4].section_type, SHT_NOBITS)

    def test_only_loaded_sections_are_hashed(self) -> None:
        hashes = identity.loaded_section_hashes(make_elf32(SAMPLE_SECTIONS))
        self.assertEqual(sorted(hashes), [".data", ".isr_vector", ".text"])
        self.assertEqual(hashes[".text"], hashlib.sha256(b"code" * 8).hexdigest())

    def test_debug_section_change_does_not_change_loaded_hashes(self) -> None:
        changed = [s if s[0] != ".debug_line" else (s[0], s[1], s[2], b"other line table")
                   for s in SAMPLE_SECTIONS]
        self.assertEqual(identity.loaded_section_hashes(make_elf32(SAMPLE_SECTIONS)),
                         identity.loaded_section_hashes(make_elf32(changed)))

    def test_big_endian_is_parsed(self) -> None:
        hashes = identity.loaded_section_hashes(make_elf32(SAMPLE_SECTIONS, order=">"))
        self.assertEqual(hashes[".data"], hashlib.sha256(b"data").hexdigest())

    def test_duplicate_section_names_get_a_suffix(self) -> None:
        dup = SAMPLE_SECTIONS + [(".text", SHT_PROGBITS, SHF_ALLOC, b"more")]
        self.assertIn(".text#2", identity.loaded_section_hashes(make_elf32(dup)))

    def test_rejects_malformed_input(self) -> None:
        good = make_elf32(SAMPLE_SECTIONS)
        for data in (b"", b"MZ" + b"\0" * 60, good[:40], good[:-8],
                     good[:4] + b"\x03" + good[5:], good[:5] + b"\x07" + good[6:]):
            with self.subTest(length=len(data)):
                with self.assertRaises(ValueError):
                    identity.parse_elf_sections(data)

    def test_section_beyond_file_is_rejected(self) -> None:
        data = bytearray(make_elf32(SAMPLE_SECTIONS))
        shoff = struct.unpack_from("<I", data, 32)[0]
        # Section 2 (.text): size field at offset 20 in the entry.
        struct.pack_into("<I", data, shoff + 2 * 40 + 20, 0x7FFFFFFF)
        with self.assertRaises(ValueError):
            identity.parse_elf_sections(bytes(data))


def entry(bin_hash: str, size: int = 100, **sections: str) -> dict:
    return {"firmware_bin_sha256": bin_hash, "firmware_bin_bytes": size, "sections": sections}


class CompareTests(unittest.TestCase):
    def test_equal_and_unequal_per_environment(self) -> None:
        ref = {"environments": {"a": entry("11" * 32, text="t"), "b": entry("22" * 32, text="t", data="d")}}
        cur = {"environments": {"a": entry("11" * 32, text="t"), "b": entry("33" * 32, 104, text="u", data="d")}}
        results = identity.compare_manifests(ref, cur, ["a", "b"])
        self.assertEqual([(r.environment, r.equal) for r in results], [("a", True), ("b", False)])
        self.assertIn("changed sections: text", results[1].detail)
        self.assertIn("100 -> 104 B", results[1].detail)

    def test_bin_decides_even_if_sections_match(self) -> None:
        ref = {"environments": {"a": entry("11" * 32, text="t")}}
        cur = {"environments": {"a": entry("44" * 32, text="t")}}
        self.assertFalse(identity.compare_manifests(ref, cur, ["a"])[0].equal)

    def test_missing_environment_is_unequal(self) -> None:
        ref = {"environments": {"a": entry("11" * 32)}}
        results = identity.compare_manifests(ref, {"environments": {}}, ["a", "b"])
        self.assertEqual([r.detail for r in results], ["missing in current build", "missing in reference"])

    def test_added_and_removed_sections_are_named(self) -> None:
        ref = {"environments": {"a": entry("11" * 32, old="x", text="t")}}
        cur = {"environments": {"a": entry("22" * 32, new="y", text="t")}}
        detail = identity.compare_manifests(ref, cur, ["a"])[0].detail
        self.assertIn("removed sections: old", detail)
        self.assertIn("new sections: new", detail)

    def test_default_environments_are_the_ten_of_d16(self) -> None:
        envs = identity.DEFAULT_ENVIRONMENTS
        self.assertEqual(len(envs), 10)
        self.assertEqual(len(set(envs)), 10)
        self.assertEqual(sum(e.startswith("release_") for e in envs), 5)
        platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        for env in envs:
            self.assertIn(f"[env:{env}]", platformio)


class CommandLineTests(unittest.TestCase):
    def test_compare_two_manifests_without_building(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            ref_path, cur_path = Path(tmp) / "ref.json", Path(tmp) / "cur.json"
            base = {"format": 1, "git": {"head": "abc", "dirty": False}}
            identity.write_manifest(ref_path, {**base, "environments": {"a": entry("11" * 32)}})
            identity.write_manifest(cur_path, {**base, "environments": {"a": entry("11" * 32)}})
            self.assertNotIn(b"\r", ref_path.read_bytes())
            self.assertEqual(identity.main(["compare", "--manifest", str(ref_path),
                                            "--against", str(cur_path), "--env", "a"]), 0)
            identity.write_manifest(cur_path, {**base, "environments": {"a": entry("22" * 32)}})
            self.assertEqual(identity.main(["compare", "--manifest", str(ref_path),
                                            "--against", str(cur_path), "--env", "a"]), 1)

    def test_unknown_manifest_format_is_refused(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "m.json"
            path.write_text(json.dumps({"format": 99}), encoding="utf-8")
            with self.assertRaises(SystemExit):
                identity.load_manifest(path)

    def test_failed_build_is_repeated_once_then_reported(self) -> None:
        calls = []

        def fake_run(command, cwd, env, stdout, stderr, check):  # noqa: ANN001
            calls.append((command, env["PLATFORMIO_BUILD_DIR"]))
            return type("Result", (), {"returncode": 1 if len(calls) == 1 else 0})()

        original = identity.subprocess.run
        identity.subprocess.run = fake_run
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                bin_path, elf_path = identity.build_environment("pio", "envx", root / "b", root, root / "logs")
                self.assertEqual(2, len(calls))
                self.assertEqual(["pio", "run", "-e", "envx"], calls[0][0])
                self.assertTrue(calls[0][1].endswith("envx"))
                self.assertEqual(root / "b" / "envx" / "envx" / "firmware.bin", bin_path)
                self.assertTrue((root / "logs" / "envx.attempt2.log").exists())
                calls.clear()
                identity.subprocess.run = lambda *a, **k: type("Result", (), {"returncode": 2})()
                with self.assertRaises(RuntimeError):
                    identity.build_environment("pio", "envy", root / "b", root, root / "logs")
        finally:
            identity.subprocess.run = original

    def test_image_record_hashes_bin_and_sections(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            bin_path, elf_path = Path(tmp) / "firmware.bin", Path(tmp) / "firmware.elf"
            bin_path.write_bytes(b"image")
            elf_path.write_bytes(make_elf32(SAMPLE_SECTIONS))
            record = identity.image_record(bin_path, elf_path)
            self.assertEqual(record["firmware_bin_sha256"], hashlib.sha256(b"image").hexdigest())
            self.assertEqual(record["firmware_bin_bytes"], 5)
            self.assertEqual(sorted(record["sections"]), [".data", ".isr_vector", ".text"])


if __name__ == "__main__":
    unittest.main()
