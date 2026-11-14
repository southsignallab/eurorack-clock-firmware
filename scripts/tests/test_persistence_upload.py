"""Tests for the persistence-preserving STM32 DFU upload helper.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import importlib.util
import re
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "scripts" / "upload_preserving_persistence.py"
SPEC = importlib.util.spec_from_file_location("upload_preserving_persistence", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("Unable to load persistence upload helper")
UPLOAD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(UPLOAD)


def committed_slot(payload: bytes, generation: int = 1, payload_size: int | None = None) -> bytes:
    """Build one committed 16-KiB physical slot exactly as PersistentStorage writes it."""
    size = len(payload) if payload_size is None else payload_size
    words = struct.pack(
        "<IIIII",
        UPLOAD.PERSIST_SLOT_MAGIC,
        UPLOAD.PERSIST_FORMAT_VERSION,
        generation,
        size,
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    header = words + struct.pack(
        "<III", zlib.crc32(words) & 0xFFFFFFFF, 0xFFFFFFFF, UPLOAD.PERSIST_COMMIT_MARKER)
    slot = header + payload
    return slot + b"\xFF" * (UPLOAD.PERSIST_SLOT_BYTES - len(slot))


ERASED_SLOT = b"\xFF" * (16 * 1024)


class PersistenceUploadTests(unittest.TestCase):
    def test_flash_regions_leave_exact_two_sector_gap(self) -> None:
        self.assertEqual(UPLOAD.BOOT_ADDRESS, 0x08000000)
        self.assertEqual(UPLOAD.BOOT_CAPACITY, 16 * 1024)
        self.assertEqual(UPLOAD.APP_ADDRESS, 0x0800C000)
        self.assertEqual(UPLOAD.APP_CAPACITY, 208 * 1024)
        self.assertEqual(
            UPLOAD.APP_ADDRESS - (UPLOAD.BOOT_ADDRESS + UPLOAD.BOOT_CAPACITY),
            32 * 1024,
        )
        self.assertEqual(UPLOAD.BOOT_CAPACITY + UPLOAD.APP_CAPACITY, 224 * 1024)

    def test_objcopy_selects_only_owned_flash_sections(self) -> None:
        command = UPLOAD.objcopy_command(
            "arm-none-eabi-objcopy",
            Path("firmware.elf"),
            Path("boot.bin"),
            UPLOAD.BOOT_SECTIONS,
        )
        self.assertIn(".isr_vector", command)
        self.assertIn(".flash_boot", command)
        self.assertNotIn(".text", command)

        app = UPLOAD.objcopy_command(
            "arm-none-eabi-objcopy",
            Path("firmware.elf"),
            Path("app.bin"),
            UPLOAD.APP_SECTIONS,
        )
        self.assertIn(".text", app)
        self.assertIn(".data", app)
        self.assertNotIn(".isr_vector", app)

    def test_dfu_commands_use_explicit_nonpersistent_addresses(self) -> None:
        tool = Path("dfu-util")
        app = UPLOAD.dfu_command(tool, Path("app.bin"), UPLOAD.APP_ADDRESS, False, "0483:df11")
        boot = UPLOAD.dfu_command(tool, Path("boot.bin"), UPLOAD.BOOT_ADDRESS, True, "0483:df11")
        self.assertIn("0x0800C000", app)
        self.assertNotIn(":leave", " ".join(app))
        self.assertIn("0x08000000:leave", boot)


    def test_cube_runtime_preserves_pre_arduino_hardware_contract(self) -> None:
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        init_pos = main.index("clockfw::hal::platform::initializeMcu();")
        app_pos = main.index("static clockfw::app::ClockApplication application;")
        self.assertLess(init_pos, app_pos)

        platform = (ROOT / "src" / "hal" / "platform_io.cpp").read_text(encoding="utf-8")
        self.assertGreaterEqual(platform.count("SCB->VTOR = FLASH_BASE;"), 2)
        self.assertIn("extern \"C\" void HAL_MspInit()", platform)
        self.assertIn("__HAL_RCC_SYSCFG_CLK_ENABLE();", platform)

        oled = (ROOT / "src" / "hal" / "oled_display_transport.cpp").read_text(encoding="utf-8")
        self.assertIn("SPI_DIRECTION_2LINES", oled)
        self.assertNotIn("SPI_DIRECTION_1LINE", oled)
        self.assertNotIn("kDisplaySpiMisoPin", oled)

    def test_region_size_guard_rejects_empty_and_oversized_images(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "image.bin"
            path.write_bytes(b"")
            with self.assertRaises(RuntimeError):
                UPLOAD.ensure_region_size(path, 16, "test")
            path.write_bytes(b"x" * 17)
            with self.assertRaises(RuntimeError):
                UPLOAD.ensure_region_size(path, 16, "test")
            path.write_bytes(b"x" * 16)
            UPLOAD.ensure_region_size(path, 16, "test")

    def test_read_command_uses_bounded_dfuse_range(self) -> None:
        command = UPLOAD.dfu_read_command(
            Path("dfu-util"), Path("readback.bin"), 0x08004000, 4128, "0483:df11")
        self.assertIn("0x08004000:4128", command)
        self.assertIn("-U", command)

    def test_slot_wrapper_matches_firmware_validation_contract(self) -> None:
        payload = b"CUR4" + (b"\xFF" * (UPLOAD.LEGACY_PAYLOAD_BYTES - 4))
        image = UPLOAD.build_slot_image(payload, generation=7)
        self.assertEqual(len(image), UPLOAD.PERSIST_HEADER_BYTES + UPLOAD.LEGACY_PAYLOAD_BYTES)
        self.assertTrue(UPLOAD.slot_image_is_valid(image))
        corrupted = bytearray(image)
        corrupted[-1] ^= 1
        self.assertFalse(UPLOAD.slot_image_is_valid(bytes(corrupted)))
        self.assertFalse(UPLOAD.slot_image_is_valid(image[:20]))
        with self.assertRaises(ValueError):
            UPLOAD.build_slot_image(b"short")

    def test_legacy_detection_rejects_blank_or_application_bytes(self) -> None:
        blank = b"\xFF" * UPLOAD.LEGACY_PAYLOAD_BYTES
        app = b"APP!" + (b"\x00" * (UPLOAD.LEGACY_PAYLOAD_BYTES - 4))
        current = b"CUR4" + (b"\xFF" * (UPLOAD.LEGACY_PAYLOAD_BYTES - 4))
        preset = bytearray(blank)
        preset[128:132] = b"PRE3"
        self.assertFalse(UPLOAD.legacy_payload_is_plausible(blank))
        self.assertFalse(UPLOAD.legacy_payload_is_plausible(app))
        self.assertTrue(UPLOAD.legacy_payload_is_plausible(current))
        self.assertTrue(UPLOAD.legacy_payload_is_plausible(bytes(preset)))
        self.assertFalse(UPLOAD.legacy_payload_is_plausible(b"short"))

    def test_every_firmware_payload_size_is_a_valid_slot(self) -> None:
        # F-001: the firmware writes 12-KiB images; 4 and 8 KiB remain readable.
        for size in (4096, 8192, 12 * 1024):
            with self.subTest(size=size):
                payload = bytes(index & 0xFF for index in range(size))
                slot = committed_slot(payload, generation=42)
                self.assertTrue(UPLOAD.slot_image_is_valid(slot))
                self.assertEqual(UPLOAD.slot_header(slot), (42, size))

    def test_slot_validation_rejects_damaged_or_foreign_slots(self) -> None:
        payload = bytes(range(256)) * 48  # 12 KiB
        good = committed_slot(payload)
        self.assertTrue(UPLOAD.slot_image_is_valid(good))
        self.assertFalse(UPLOAD.slot_image_is_valid(ERASED_SLOT))
        corrupt_payload = bytearray(good)
        corrupt_payload[UPLOAD.PERSIST_HEADER_BYTES + 100] ^= 0x01
        self.assertFalse(UPLOAD.slot_image_is_valid(bytes(corrupt_payload)))
        uncommitted = bytearray(good)
        uncommitted[28:32] = b"\xFF\xFF\xFF\xFF"
        self.assertFalse(UPLOAD.slot_image_is_valid(bytes(uncommitted)))
        bad_header_crc = bytearray(good)
        bad_header_crc[20] ^= 0x01
        self.assertFalse(UPLOAD.slot_image_is_valid(bytes(bad_header_crc)))
        unsupported = committed_slot(bytes(2048))
        self.assertFalse(UPLOAD.slot_image_is_valid(unsupported))
        self.assertFalse(UPLOAD.slot_image_is_valid(good[:UPLOAD.PERSIST_HEADER_BYTES + 4096]))

    def test_readback_split_requires_two_full_sectors(self) -> None:
        slot_a = committed_slot(bytes(12 * 1024), generation=3)
        slot_b = committed_slot(bytes(8192), generation=2)
        first, second = UPLOAD.split_persistence_readback(slot_a + slot_b)
        self.assertEqual((first, second), (slot_a, slot_b))
        with self.assertRaises(RuntimeError):
            UPLOAD.split_persistence_readback(slot_a)

    def test_backup_names_never_collide(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            first = UPLOAD.backup_path(Path(directory), now=0.0)
            first.write_bytes(b"x")
            second = UPLOAD.backup_path(Path(directory), now=0.0)
            self.assertNotEqual(first, second)
            self.assertTrue(second.name.startswith("persistence-"))

    def test_slot_contract_matches_firmware_sources(self) -> None:
        # The slot format constants live in the internal slot header since P2 (F-009/F-045).
        storage = (ROOT / "src" / "hal" / "persistent_storage_slots.h").read_text(encoding="utf-8")
        storage_h = (ROOT / "src" / "hal" / "persistent_storage.h").read_text(encoding="utf-8")
        layout = (ROOT / "src" / "hal" / "persistent_layout.h").read_text(encoding="utf-8")

        def constant(source: str, name: str) -> str:
            match = re.search(rf"\b{name}\s*=\s*([^;]+);", source)
            self.assertIsNotNone(match, name)
            assert match is not None
            return match.group(1).strip()

        self.assertEqual(int(constant(storage, "kSlotMagic").rstrip("UL"), 16), UPLOAD.PERSIST_SLOT_MAGIC)
        self.assertEqual(int(constant(storage, "kSlotFormatVersion").rstrip("U")),
                         UPLOAD.PERSIST_FORMAT_VERSION)
        self.assertEqual(int(constant(storage, "kCommitMarker").rstrip("UL"), 16),
                         UPLOAD.PERSIST_COMMIT_MARKER)
        self.assertEqual(int(constant(storage, "kSlotHeaderBytes").rstrip("U")),
                         UPLOAD.PERSIST_HEADER_BYTES)
        self.assertEqual(int(constant(storage, "kLegacyPayloadBytes").rstrip("U")),
                         UPLOAD.LEGACY_PAYLOAD_BYTES)
        self.assertIn("16U * 1024U", constant(storage_h, "kPhysicalSlotBytes"))
        self.assertEqual(UPLOAD.PERSIST_SLOT_BYTES, 16 * 1024)
        self.assertEqual(int(constant(layout, "kV1ImageBytes").rstrip("U")), UPLOAD.V1_PAYLOAD_BYTES)
        self.assertIn("12U * 1024U", constant(layout, "kMaximumImageBytes"))
        self.assertEqual(constant(layout, "kCurrentImageBytes"), "kMaximumImageBytes")
        self.assertEqual(UPLOAD.CURRENT_PAYLOAD_BYTES, 12 * 1024)
        self.assertEqual(
            sorted(UPLOAD.PERSIST_PAYLOAD_SIZES),
            [UPLOAD.LEGACY_PAYLOAD_BYTES, UPLOAD.V1_PAYLOAD_BYTES, UPLOAD.CURRENT_PAYLOAD_BYTES],
        )

    def _run_main(self, persistence: bytes, legacy: bytes, *extra: str) -> tuple[int, list[list[str]], Path]:
        """Run main() against a fake DFU device; return exit code, commands and backup dir."""
        commands: list[list[str]] = []
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        base = Path(directory.name)
        elf = base / "build" / "firmware.elf"
        elf.parent.mkdir()
        elf.write_bytes(b"ELF")
        backups = base / "backups"

        def fake_run(command: list[str]) -> None:
            commands.append(list(command))
            if "-U" in command:
                output = Path(command[command.index("-U") + 1])
                address = command[command.index("-s") + 1]
                self.assertFalse(output.exists(), "dfu-util must not overwrite a readback")
                output.write_bytes(persistence if address.startswith("0x08004000") else legacy)
            elif command[0] == "objcopy":
                Path(command[-1]).write_bytes(b"\x00" * 64)

        argv = ["upload", "--elf", str(elf), "--objcopy", "objcopy",
                "--packages-dir", str(base), "--backup-dir", str(backups), *extra]
        with mock.patch.object(UPLOAD, "run_checked", side_effect=fake_run), \
                mock.patch.object(UPLOAD, "find_dfu_tool", side_effect=lambda _p, name: Path(name)), \
                mock.patch.object(sys, "argv", argv):
            result = UPLOAD.main()
        return result, commands, backups

    @staticmethod
    def _downloads_to(commands: list[list[str]], address: str) -> list[list[str]]:
        return [c for c in commands if "-D" in c and c[c.index("-s") + 1].startswith(address)]

    def test_upload_keeps_full_backup_and_never_touches_valid_slots(self) -> None:
        current = committed_slot(bytes(12 * 1024), generation=9)
        legacy = b"PRE4" + b"\x00" * (UPLOAD.LEGACY_PAYLOAD_BYTES - 4)
        result, commands, backups = self._run_main(current + ERASED_SLOT, legacy,
                                                   "--migrate-legacy-eeprom")
        self.assertEqual(result, 0)
        reads = [c for c in commands if "-U" in c]
        self.assertEqual([c[c.index("-s") + 1] for c in reads], ["0x08004000:32768"])
        self.assertEqual(self._downloads_to(commands, "0x08004000"), [])
        saved = list(backups.glob("persistence-*.bin"))
        self.assertEqual(len(saved), 1)
        self.assertEqual(saved[0].read_bytes(), current + ERASED_SLOT)
        self.assertEqual(len(self._downloads_to(commands, "0x0800C000")), 1)
        self.assertEqual(len(self._downloads_to(commands, "0x08000000")), 1)

    def test_legacy_migration_needs_explicit_switch(self) -> None:
        legacy = b"\x00" * 128 + b"PRE4" + b"\x00" * (UPLOAD.LEGACY_PAYLOAD_BYTES - 132)
        result, commands, _ = self._run_main(ERASED_SLOT + ERASED_SLOT, legacy)
        self.assertEqual(result, 0)
        self.assertEqual(self._downloads_to(commands, "0x08004000"), [])
        self.assertFalse(any("0x08020000" in " ".join(c) for c in commands))

        result, commands, _ = self._run_main(ERASED_SLOT + ERASED_SLOT, legacy,
                                             "--migrate-legacy-eeprom")
        self.assertEqual(result, 0)
        self.assertEqual(len(self._downloads_to(commands, "0x08004000")), 1)

    def test_platformio_forwards_upload_flags(self) -> None:
        config = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        command = next(line for line in config.splitlines() if line.startswith("upload_command"))
        self.assertIn("$UPLOAD_FLAGS", command)

    def test_finds_posix_and_windows_dfu_tools(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            package = Path(directory) / "tool-dfuutil" / "bin"
            package.mkdir(parents=True)
            posix = package / "dfu-util"
            posix.write_text("", encoding="utf-8")
            self.assertEqual(UPLOAD.find_dfu_tool(Path(directory), "dfu-util"), posix)
            posix.unlink()
            windows = package / "dfu-util.exe"
            windows.write_text("", encoding="utf-8")
            self.assertEqual(UPLOAD.find_dfu_tool(Path(directory), "dfu-util"), windows)
            windows.unlink()
            with self.assertRaises(FileNotFoundError):
                UPLOAD.find_dfu_tool(Path(directory), "dfu-util")

    def test_platformio_uses_custom_persistence_preserving_uploader(self) -> None:
        config = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("upload_protocol = custom", config)
        self.assertIn("upload_preserving_persistence.py", config)
        self.assertIn("board_upload.maximum_size = 229376", config)
        self.assertIn("board_build.ldscript = ld/stm32f401cc_clock.ld", config)
        linker = (ROOT / "ld" / "stm32f401cc_clock.ld").read_text(encoding="utf-8")
        self.assertIn("ORIGIN = 0x20000000, LENGTH = 64K", linker)
        self.assertNotIn("LD_MAX_DATA_SIZE", linker)
        self.assertNotIn("(READONLY)", linker)


if __name__ == "__main__":
    unittest.main()
