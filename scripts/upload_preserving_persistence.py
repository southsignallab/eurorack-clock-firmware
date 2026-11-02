#!/usr/bin/env python3
"""Upload the split STM32F401 image without touching persistent Flash sectors.

The clock linker layout intentionally leaves sectors 1 and 2 out of the ELF:

    0x08000000..0x08003FFF  sector 0   boot/vector image
    0x08004000..0x08007FFF  sector 1   persistence A
    0x08008000..0x0800BFFF  sector 2   persistence B
    0x0800C000..0x0803FFFF  sectors 3-5 application image

A normal raw firmware.bin would fill the address gap and DFU would overwrite the
persistent sectors. This helper extracts and uploads the two load regions
separately so firmware updates preserve settings and presets.

Before anything is written, both persistence sectors are read back in full and
kept as a timestamped backup (default ``.pio/persistence-backups/``). A slot is
recognized with any payload size the firmware accepts (4, 8 or 12 KiB). The
one-time migration from the pre-A/B STM32duino EEPROM layout runs only with
``--migrate-legacy-eeprom``, because that address now belongs to the
application image.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0
"""
from __future__ import annotations

import argparse
import struct
import subprocess
import sys
import time
import zlib
from pathlib import Path
from typing import Sequence

BOOT_ADDRESS = 0x08000000
BOOT_CAPACITY = 16 * 1024
APP_ADDRESS = 0x0800C000
APP_CAPACITY = 208 * 1024
DEFAULT_USB_ID = "0483:df11"
PERSIST_A_ADDRESS = 0x08004000
PERSIST_B_ADDRESS = 0x08008000
PERSIST_SLOT_BYTES = 16 * 1024
LEGACY_EEPROM_ADDRESS = 0x08020000
# Payload sizes accepted by PersistentStorage::slotIsValid(): legacy 4 KiB,
# V1 8 KiB (persistent_layout::kV1ImageBytes) and the current 12 KiB image
# (persistent_layout::kCurrentImageBytes).
LEGACY_PAYLOAD_BYTES = 4096
V1_PAYLOAD_BYTES = 8192
CURRENT_PAYLOAD_BYTES = 12 * 1024
PERSIST_PAYLOAD_SIZES = (LEGACY_PAYLOAD_BYTES, V1_PAYLOAD_BYTES, CURRENT_PAYLOAD_BYTES)
PERSIST_HEADER_BYTES = 32
PERSIST_SLOT_MAGIC = 0x434C4B50
PERSIST_FORMAT_VERSION = 1
PERSIST_COMMIT_MARKER = 0xC10C17ED
DEFAULT_BACKUP_DIR = Path(__file__).resolve().parents[1] / ".pio" / "persistence-backups"

BOOT_SECTIONS = (".isr_vector", ".flash_boot")
APP_SECTIONS = (
    ".text",
    ".rodata",
    ".ARM.extab",
    ".ARM",
    ".preinit_array",
    ".init_array",
    ".fini_array",
    ".data",
)


def _section_args(sections: Sequence[str]) -> list[str]:
    args: list[str] = []
    for section in sections:
        args.extend(("--only-section", section))
    return args


def objcopy_command(objcopy: str, elf: Path, output: Path, sections: Sequence[str]) -> list[str]:
    """Build one deterministic objcopy command for a selected load region."""
    return [objcopy, "-O", "binary", *_section_args(sections), str(elf), str(output)]


def dfu_command(dfu_util: Path, image: Path, address: int, leave: bool, usb_id: str) -> list[str]:
    """Build one dfu-util download command for an explicit Flash address."""
    address_arg = f"0x{address:08X}" + (":leave" if leave else "")
    return [
        str(dfu_util),
        "-d", usb_id,
        "-a", "0",
        "-s", address_arg,
        "-D", str(image),
    ]


def dfu_read_command(
    dfu_util: Path,
    output: Path,
    address: int,
    size: int,
    usb_id: str,
) -> list[str]:
    """Build one DfuSe upload command that reads a bounded Flash range."""
    return [
        str(dfu_util),
        "-d", usb_id,
        "-a", "0",
        "-s", f"0x{address:08X}:{size}",
        "-U", str(output),
    ]


def build_slot_image(payload: bytes, generation: int = 1) -> bytes:
    """Wrap one legacy 4-KiB logical image in the firmware's committed A/B slot format."""
    if len(payload) != LEGACY_PAYLOAD_BYTES:
        raise ValueError(f"Expected {LEGACY_PAYLOAD_BYTES} persistence bytes")
    payload_crc = zlib.crc32(payload) & 0xFFFFFFFF
    first_five_words = struct.pack(
        "<IIIII",
        PERSIST_SLOT_MAGIC,
        PERSIST_FORMAT_VERSION,
        generation & 0xFFFFFFFF,
        LEGACY_PAYLOAD_BYTES,
        payload_crc,
    )
    header_crc = zlib.crc32(first_five_words) & 0xFFFFFFFF
    header = first_five_words + struct.pack(
        "<III",
        header_crc,
        0xFFFFFFFF,
        PERSIST_COMMIT_MARKER,
    )
    if len(header) != PERSIST_HEADER_BYTES:
        raise AssertionError("Unexpected persistence header size")
    return header + payload


def slot_header(slot: bytes) -> tuple[int, int] | None:
    """Return (generation, payload_size) for a committed, CRC-valid slot, else None.

    Mirrors PersistentStorage::slotIsValid(): magic, format version, accepted
    payload size, COMMIT marker, header CRC over the first five words and payload
    CRC. zlib.crc32 is the same reflected CRC-32 (0xEDB88320) the firmware uses.
    """
    if len(slot) < PERSIST_HEADER_BYTES:
        return None
    words = struct.unpack("<IIIIIIII", slot[:PERSIST_HEADER_BYTES])
    magic, version, generation, payload_size, payload_crc, header_crc, _reserved, commit = words
    if (
        magic != PERSIST_SLOT_MAGIC
        or version != PERSIST_FORMAT_VERSION
        or payload_size not in PERSIST_PAYLOAD_SIZES
        or commit != PERSIST_COMMIT_MARKER
    ):
        return None
    if (zlib.crc32(slot[:20]) & 0xFFFFFFFF) != header_crc:
        return None
    if len(slot) < PERSIST_HEADER_BYTES + payload_size:
        return None
    payload = slot[PERSIST_HEADER_BYTES:PERSIST_HEADER_BYTES + payload_size]
    if (zlib.crc32(payload) & 0xFFFFFFFF) != payload_crc:
        return None
    return generation, payload_size


def slot_image_is_valid(slot: bytes) -> bool:
    """Validate one slot readback using the firmware's exact slot contract."""
    return slot_header(slot) is not None


def split_persistence_readback(data: bytes) -> tuple[bytes, bytes]:
    """Split one contiguous sector-1/sector-2 readback into slot A and slot B."""
    if len(data) != 2 * PERSIST_SLOT_BYTES:
        raise RuntimeError(
            f"Persistence readback is {len(data)} bytes, expected {2 * PERSIST_SLOT_BYTES}")
    return data[:PERSIST_SLOT_BYTES], data[PERSIST_SLOT_BYTES:]


def describe_slot(label: str, slot: bytes) -> str:
    """Human-readable one-line slot status for the upload log."""
    header = slot_header(slot)
    if header is None:
        return f"{label}: no valid committed image"
    generation, payload_size = header
    return f"{label}: valid, generation {generation}, payload {payload_size} bytes"


def backup_path(backup_dir: Path, now: float | None = None) -> Path:
    """Timestamped, collision-free backup file name (dfu-util will not overwrite)."""
    stamp = time.strftime("%Y%m%d-%H%M%S", time.localtime(now))
    candidate = backup_dir / f"persistence-{stamp}.bin"
    suffix = 1
    while candidate.exists():
        candidate = backup_dir / f"persistence-{stamp}-{suffix}.bin"
        suffix += 1
    return candidate


def legacy_payload_is_plausible(payload: bytes) -> bool:
    """Recognize the pre-A/B logical image without pretending arbitrary Flash is state."""
    if len(payload) != LEGACY_PAYLOAD_BYTES or all(byte == 0xFF for byte in payload):
        return False
    record_magics = (b"CUR3", b"CUR4", b"PRE3", b"PRE4")
    return payload[:4] in record_magics or any(magic in payload for magic in record_magics[2:])


def find_dfu_tool(packages_dir: Path, executable: str) -> Path:
    """Resolve PlatformIO's bundled dfu-util/dfu-suffix on Windows or POSIX."""
    tool_dir = packages_dir / "tool-dfuutil" / "bin"
    candidates = (tool_dir / executable, tool_dir / f"{executable}.exe")
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f"Unable to find {executable} below {tool_dir}")


def ensure_region_size(path: Path, capacity: int, label: str) -> None:
    """Reject a split image that would cross its physical Flash region."""
    size = path.stat().st_size
    if size == 0:
        raise RuntimeError(f"{label} image is empty: {path}")
    if size > capacity:
        raise RuntimeError(
            f"{label} image is {size} bytes but its region is only {capacity} bytes")


def run_checked(command: Sequence[str]) -> None:
    """Run one external tool and propagate a non-zero result."""
    subprocess.run(list(command), check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--objcopy", required=True)
    parser.add_argument("--packages-dir", required=True)
    parser.add_argument("--usb-id", default=DEFAULT_USB_ID)
    parser.add_argument(
        "--backup-dir",
        default=str(DEFAULT_BACKUP_DIR),
        help="directory for the timestamped sector-1/2 backup taken before every upload",
    )
    parser.add_argument(
        "--migrate-legacy-eeprom",
        action="store_true",
        help="one-time upgrade from the pre-A/B STM32duino EEPROM layout at 0x08020000; "
             "only used when neither A/B slot is valid",
    )
    args = parser.parse_args()

    elf = Path(args.elf).resolve()
    if not elf.is_file():
        raise SystemExit(f"ELF not found: {elf}")

    build_dir = elf.parent
    boot_bin = build_dir / "firmware-boot.bin"
    app_bin = build_dir / "firmware-app.bin"
    legacy_readback = build_dir / ".legacy-eeprom-readback.bin"
    migrated_slot = build_dir / ".persist-migration-slot.bin"

    try:
        dfu_util = find_dfu_tool(Path(args.packages_dir), "dfu-util")
        dfu_suffix = find_dfu_tool(Path(args.packages_dir), "dfu-suffix")

        run_checked(objcopy_command(args.objcopy, elf, boot_bin, BOOT_SECTIONS))
        run_checked(objcopy_command(args.objcopy, elf, app_bin, APP_SECTIONS))
        ensure_region_size(boot_bin, BOOT_CAPACITY, "boot")
        ensure_region_size(app_bin, APP_CAPACITY, "application")

        # Read both physical persistence sectors in full and keep the readback
        # as a backup before anything is written. Both slots are judged on the
        # complete 16-KiB sector so every accepted payload size is covered.
        backup_dir = Path(args.backup_dir)
        backup_dir.mkdir(parents=True, exist_ok=True)
        backup = backup_path(backup_dir)
        run_checked(dfu_read_command(
            dfu_util, backup, PERSIST_A_ADDRESS, 2 * PERSIST_SLOT_BYTES, args.usb_id))
        slot_a, slot_b = split_persistence_readback(backup.read_bytes())
        print(f"Persistence backup: {backup}")
        print(describe_slot("Slot A", slot_a))
        print(describe_slot("Slot B", slot_b))
        have_new_persistence = slot_image_is_valid(slot_a) or slot_image_is_valid(slot_b)

        # Existing A/B state always wins. The old sector-5 EEPROM image is only
        # considered on explicit request: sector 5 now holds application code,
        # so a heuristic match there must never seed slot A on its own.
        if not have_new_persistence and not args.migrate_legacy_eeprom:
            print("No valid A/B persistence slot; legacy EEPROM migration skipped "
                  "(pass --migrate-legacy-eeprom for a pre-A/B upgrade).")
        if not have_new_persistence and args.migrate_legacy_eeprom:
            run_checked(dfu_read_command(
                dfu_util,
                legacy_readback,
                LEGACY_EEPROM_ADDRESS,
                LEGACY_PAYLOAD_BYTES,
                args.usb_id,
            ))
            legacy_payload = legacy_readback.read_bytes()
            if not legacy_payload_is_plausible(legacy_payload):
                print("Legacy EEPROM region holds no recognizable state; nothing migrated.")
            else:
                print("Migrating legacy EEPROM image into slot A.")
                migrated_slot.write_bytes(build_slot_image(legacy_payload))
                vid, pid = args.usb_id.split(":", maxsplit=1)
                run_checked([
                    str(dfu_suffix),
                    "-v", f"0x{vid}",
                    "-p", f"0x{pid}",
                    "-d", "0xffff",
                    "-a", str(migrated_slot),
                ])
                run_checked(dfu_command(
                    dfu_util,
                    migrated_slot,
                    PERSIST_A_ADDRESS,
                    False,
                    args.usb_id,
                ))

        # Match PlatformIO's STM32 DFU behavior: attach the ST VID/PID suffix to
        # each raw image before handing it to dfu-util.
        vid, pid = args.usb_id.split(":", maxsplit=1)
        for image in (boot_bin, app_bin):
            run_checked([
                str(dfu_suffix),
                "-v", f"0x{vid}",
                "-p", f"0x{pid}",
                "-d", "0xffff",
                "-a", str(image),
            ])

        # Program the application region first and the vector-table sector last.
        # DfuSe :leave must point at the image that contains the initial SP and
        # Reset_Handler vector, not at the application code region. This also
        # avoids exposing a newly written vector table before its target code is
        # present when an update is interrupted.
        run_checked(dfu_command(dfu_util, app_bin, APP_ADDRESS, False, args.usb_id))
        run_checked(dfu_command(dfu_util, boot_bin, BOOT_ADDRESS, True, args.usb_id))
    except (FileNotFoundError, RuntimeError, subprocess.CalledProcessError, ValueError) as exc:
        print(f"Persistence-preserving DFU upload failed: {exc}", file=sys.stderr)
        return 1
    finally:
        # Split images are upload intermediates. The ELF remains the canonical
        # sparse build artifact; do not leave confusing raw binaries around.
        for image in (
            boot_bin,
            app_bin,
            legacy_readback,
            migrated_slot,
        ):
            try:
                image.unlink()
            except FileNotFoundError:
                pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
