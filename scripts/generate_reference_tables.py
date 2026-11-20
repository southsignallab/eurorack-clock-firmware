#!/usr/bin/env python3
"""Generate reference tables in the documentation from the firmware sources.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0

Each table lives between two marker comments in its authoritative document and is rewritten
from the code. ``--check`` fails when a document differs from the code, so a code change that
alters a table cannot pass the documentation check without the matching document update.

Tables:
  pins            docs/CONFIGURATION.md   from src/pin_map.h
  display-macros  docs/CONFIGURATION.md   from src/pin_map.h and src/config.h

Usage (repository root):
  python scripts/generate_reference_tables.py           # rewrite the tables
  python scripts/generate_reference_tables.py --check   # report stale tables, change nothing
"""
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIN_MAP = ROOT / "src" / "pin_map.h"
CONFIG = ROOT / "src" / "config.h"
CONFIGURATION_DOC = ROOT / "docs" / "CONFIGURATION.md"

PIN_RE = re.compile(r"\bmcu::(P[A-H](?:1[0-5]|[0-9]))\b")
CONSTANT_RE = re.compile(r"^inline constexpr std::uint32_t (k\w+Pin) = ([^;]+);", re.MULTILINE)
DEFAULT_DEFINE_RE = re.compile(r"^#ifndef (CLOCK_\w+)\n#define \1 ([^\n]+)\n#endif", re.MULTILINE)

#: Function text per pin constant. A constant in src/pin_map.h without an entry here stops the
#: generator, so a new signal cannot slip past the documentation.
PIN_FUNCTIONS: dict[str, str | None] = {
    "kEncoderPhaseAPin": "Encoder phase A / CLK (TIM4_CH1)",
    "kEncoderPhaseBPin": "Encoder phase B / DT (TIM4_CH2)",
    "kEncoderPushButtonPin": "Encoder push",
    "kPlayPauseButtonPin": "START/PAUSE button",
    "kTapTempoButtonPin": "TAP TEMPO button",
    "kResetBackButtonPin": "STOP/BACK button",
    "kChannel1GateLedPin": "Gate/LED channel 1",
    "kChannel2GateLedPin": "Gate/LED channel 2",
    "kChannel3GateLedPin": "Gate/LED channel 3",
    "kChannel4GateLedPin": "Gate/LED channel 4",
    "kChannel5GateLedPin": "Gate/LED channel 5",
    "kChannel6GateLedPin": "Gate/LED channel 6",
    "kChannel7GateLedPin": "Gate/LED channel 7",
    "kChannel8GateLedPin": "Gate/LED channel 8",
    "kGateBufferOutputEnablePin": "Gate-buffer `/OE`",
    "kDisplaySpiClockPin": "OLED clock (`CLK`; `SCL` on the prototype module)",
    "kDisplaySpiDataPin": "OLED data (`DIN`; SPI MOSI; `SDA` on the prototype module)",
    "kDisplaySpiChipSelectPin": "OLED SPI CS",
    "kDisplaySpiDataCommandPin": "OLED data/command (`DC`)",
    "kDisplayResetPin": "OLED reset (`RES`)",
    "kExternalSyncSignalPin": "IN 1 comparator input (SYNC net)",
    "kExternalResetSignalPin": "IN 2 comparator input (RST net)",
    # Not connected in the final pin map; listed only through the /OE row or not at all.
    "kUnassignedDigitalPin": None,
    "kDisplayI2cDataPin": None,
    "kDisplayI2cClockPin": None,
    "kExternalSyncJackDetectPin": None,
    "kExternalResetJackDetectPin": None,
}

#: Build macros whose defaults are documented, in table order.
DISPLAY_MACROS = (
    "CLOCK_DISPLAY_SPI_SCK_PIN",
    "CLOCK_DISPLAY_SPI_MOSI_PIN",
    "CLOCK_DISPLAY_SPI_CS_PIN",
    "CLOCK_DISPLAY_SPI_DC_PIN",
    "CLOCK_DISPLAY_RESET_PIN",
    "CLOCK_DISPLAY_SPI_FREQUENCY_HZ",
)


@dataclass(frozen=True)
class Table:
    """One generated table: its name, document and Markdown body."""

    name: str
    document: Path
    body: str


def defaults(*sources: str) -> dict[str, str]:
    """Return the default value of every ``#ifndef X / #define X value / #endif`` block."""
    values: dict[str, str] = {}
    for source in sources:
        for name, value in DEFAULT_DEFINE_RE.findall(source):
            values[name] = value.strip()
    return values


def resolve_pin(expression: str, macro_defaults: dict[str, str], constants: dict[str, str]) -> str | None:
    """Resolve a pin expression to ``PXn`` or None for an unassigned signal."""
    expression = expression.strip()
    for _ in range(4):
        match = PIN_RE.search(expression)
        if match:
            return match.group(1)
        if expression in macro_defaults:
            expression = macro_defaults[expression]
        elif expression in constants:
            expression = constants[expression]
        else:
            break
    if "kUnassigned" in expression or expression.upper().startswith("0XFFFFFFFF"):
        return None
    raise ValueError(f"cannot resolve pin expression {expression!r}")


def pin_sort_key(pin: str | None) -> tuple[str, int]:
    """Sort by port letter, then pin number; unassigned rows last."""
    if pin is None:
        return ("Z", 99)
    return (pin[1], int(pin[2:]))


def pin_table(pin_map: str) -> str:
    """Return the pin table body generated from src/pin_map.h."""
    constants = dict(CONSTANT_RE.findall(pin_map))
    macro_defaults = defaults(pin_map)
    unknown = sorted(set(constants) - set(PIN_FUNCTIONS))
    if unknown:
        raise ValueError(f"src/pin_map.h: no documentation text for {', '.join(unknown)}; "
                         "add it to PIN_FUNCTIONS in scripts/generate_reference_tables.py")
    rows = []
    for name, function in PIN_FUNCTIONS.items():
        if function is None:
            continue
        if name not in constants:
            raise ValueError(f"src/pin_map.h: documented pin constant {name} no longer exists")
        pin = resolve_pin(constants[name], macro_defaults, constants)
        rows.append((pin_sort_key(pin), function, pin or "not MCU-controlled in the final pin map"))
    lines = ["| Function | STM32 pin |", "| --- | --- |"]
    lines += [f"| {function} | {pin} |" for _, function, pin in sorted(rows)]
    return "\n".join(lines)


def display_macro_table(pin_map: str, config: str) -> str:
    """Return the table of display build macros and their defaults."""
    values = defaults(pin_map, config)
    lines = ["| Build macro | Default |", "| --- | --- |"]
    for macro in DISPLAY_MACROS:
        if macro not in values:
            raise ValueError(f"default of {macro} not found in src/pin_map.h or src/config.h")
        value = values[macro]
        pin = PIN_RE.search(value)
        shown = pin.group(1) if pin else re.sub(r"(?i)u?l+$", "", value)
        lines.append(f"| `{macro}` | `{shown}` |")
    return "\n".join(lines)


def tables(root: Path = ROOT) -> list[Table]:
    """Return every generated table."""
    pin_map = (root / "src" / "pin_map.h").read_text(encoding="utf-8")
    config = (root / "src" / "config.h").read_text(encoding="utf-8")
    document = root / "docs" / "CONFIGURATION.md"
    return [
        Table("pins", document, pin_table(pin_map)),
        Table("display-macros", document, display_macro_table(pin_map, config)),
    ]


def markers(name: str) -> tuple[str, str]:
    """Return the begin and end marker comments of a table."""
    return (f"<!-- generated:{name} begin (scripts/generate_reference_tables.py) -->",
            f"<!-- generated:{name} end -->")


def replace_table(text: str, table: Table) -> str:
    """Return the document text with the table between its markers replaced."""
    begin, end = markers(table.name)
    if text.count(begin) != 1 or text.count(end) != 1:
        raise ValueError(f"{table.document.name}: markers for table {table.name!r} missing or repeated")
    head, rest = text.split(begin, 1)
    _, tail = rest.split(end, 1)
    newline = "\r\n" if "\r\n" in text else "\n"  # keep the line endings of the checkout
    body = table.body.replace("\n", newline)
    return f"{head}{begin}{newline}{body}{newline}{end}{tail}"


def run(check: bool, root: Path = ROOT) -> list[str]:
    """Rewrite or check every table; return the names of stale tables."""
    stale: list[str] = []
    updated: dict[Path, str] = {}
    for table in tables(root):
        current = updated.get(table.document) or table.document.read_bytes().decode("utf-8")
        new = replace_table(current, table)
        if new != current:
            stale.append(f"{table.document.relative_to(root).as_posix()}: {table.name}")
        updated[table.document] = new
    if not check:
        for document, text in updated.items():
            document.write_bytes(text.encode("utf-8"))
    return stale


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--check", action="store_true", help="report stale tables without writing")
    args = parser.parse_args(argv)
    try:
        stale = run(args.check)
    except ValueError as exc:
        print(f"reference tables: {exc}", file=sys.stderr)
        return 2
    if args.check and stale:
        print("reference tables out of date (run scripts/generate_reference_tables.py): " + "; ".join(stale),
              file=sys.stderr)
        return 1
    print(f"reference tables {'checked' if args.check else 'written'}: {len(tables())}"
          + (f", updated: {'; '.join(stale)}" if stale and not args.check else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
