#!/usr/bin/env python3
"""Generate reference tables in the documentation from the firmware sources.

Author: Axel Napolitano
License: PolyForm-Noncommercial-1.0.0

Each table lives between two marker comments in its authoritative document and is rewritten
from the code. ``--check`` fails when a document differs from the code, so a code change that
alters a table cannot pass the documentation check without the matching document update.

Tables:
  pins                  docs/CONFIGURATION.md   from src/pin_map.h
  display-macros        docs/CONFIGURATION.md   from src/pin_map.h and src/config.h
  persistence-map       docs/ARCHITECTURE.md    from src/hal/persistent_layout.h and the stores
  interrupt-priorities  docs/TIMING.md          from src/config.h and src/hal/platform_io.cpp
  timing-constants      docs/TIMING.md          from src/config.h

Usage (repository root):
  python scripts/generate_reference_tables.py           # rewrite the tables
  python scripts/generate_reference_tables.py --check   # report stale tables, change nothing
"""
from __future__ import annotations

import argparse
import ast
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
INTEGER_CONSTANT_RE = re.compile(r"\bconstexpr\s+(?:std::)?(?:u?int\w*|size_t)\s+(k\w+)\s*=\s*([^;{]+);")
INTEGER_SUFFIX_RE = re.compile(r"\b(\d+)(?:ULL|UL|LL|U|L)\b", re.IGNORECASE)
CAST_RE = re.compile(r"static_cast<[^<>]+>\(")
QUALIFIER_RE = re.compile(r"\b(?:\w+::)+(?=k\w)")

PERSISTENT_LAYOUT = "src/hal/persistent_layout.h"
CUSTOM_GROOVE = "src/domain/custom_groove.h"
CUSTOM_GROOVE_STORE = "src/services/custom_groove_store.h"
PERSISTENT_STATE_SERVICE = "src/services/persistent_state_service.h"
#: File that defines the EXTI preemption priority of the SYNC/RST capture interrupts.
EXTI_PRIORITY_SOURCE = "src/hal/platform_io.cpp"

#: Every file the generator reads, relative to the repository root.
SOURCES = ("src/pin_map.h", "src/config.h", PERSISTENT_LAYOUT, CUSTOM_GROOVE, CUSTOM_GROOVE_STORE,
           PERSISTENT_STATE_SERVICE, EXTI_PRIORITY_SOURCE)
#: Every document that holds a generated table.
DOCUMENTS = ("docs/CONFIGURATION.md", "docs/ARCHITECTURE.md", "docs/TIMING.md")

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


def evaluate(expression: str, names: dict[str, int]) -> int:
    """Evaluate a C++ integer constant expression of literals, names and ``+ - * / ( )``.

    Integer suffixes, ``static_cast<...>`` and namespace or class qualifiers are dropped;
    division is integer division as in C++ for the non-negative values used here.
    """
    text = INTEGER_SUFFIX_RE.sub(r"\1", CAST_RE.sub("(", QUALIFIER_RE.sub("", expression.strip())))
    try:
        tree = ast.parse(text, mode="eval")
    except SyntaxError as exc:
        raise ValueError(f"unsupported constant expression {expression!r}") from exc
    operators = {ast.Add: lambda a, b: a + b, ast.Sub: lambda a, b: a - b,
                 ast.Mult: lambda a, b: a * b, ast.Div: lambda a, b: a // b}

    def walk(node: ast.AST) -> int:
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.Name):
            if node.id not in names:
                raise ValueError(f"unknown constant {node.id} in {expression!r}")
            return names[node.id]
        if isinstance(node, ast.BinOp) and type(node.op) in operators:
            return operators[type(node.op)](walk(node.left), walk(node.right))
        raise ValueError(f"unsupported constant expression {expression!r}")

    return walk(tree.body)


def integer_constants(source: str, inherited: dict[str, int] | None = None) -> dict[str, int]:
    """Return the integer ``constexpr`` constants of one source file that can be evaluated.

    Names resolve against the file itself first and then against ``inherited``; constants
    whose expression is not an integer expression of known names are left out.
    """
    pending = dict(INTEGER_CONSTANT_RE.findall(source))
    known: dict[str, int] = {}
    progress = True
    while pending and progress:
        progress = False
        for name, expression in list(pending.items()):
            try:
                known[name] = evaluate(expression, {**(inherited or {}), **known})
            except ValueError:
                continue
            del pending[name]
            progress = True
    return known


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


def read(root: Path, relative: str) -> str:
    """Return one source file of the repository at ``root``."""
    return (root / relative).read_text(encoding="utf-8")


def required(constants: dict[str, int], name: str, source: str) -> int:
    """Return one evaluated constant or stop with the file that should define it."""
    if name not in constants:
        raise ValueError(f"{source}: integer constant {name} not found")
    return constants[name]


def persistence_map(root: Path = ROOT) -> str:
    """Return the logical persistence-image map generated from the layout header and stores."""
    layout = integer_constants(read(root, PERSISTENT_LAYOUT))
    groove = integer_constants(read(root, CUSTOM_GROOVE))
    store = integer_constants(read(root, CUSTOM_GROOVE_STORE), layout)
    state = integer_constants(read(root, PERSISTENT_STATE_SERVICE), layout)

    def at(name: str) -> int:
        return required(layout, name, PERSISTENT_LAYOUT)

    current = required(state, "kCurrentRecordBytes", PERSISTENT_STATE_SERVICE)
    preset = required(state, "kPresetRecordBytes", PERSISTENT_STATE_SERVICE)
    presets = required(state, "kUserPresetSlotCount", PERSISTENT_STATE_SERVICE)
    groove_count = required(groove, "kCustomGrooveSlotCount", CUSTOM_GROOVE)
    groove_offset = at("kCustomGrooveRegionOffset")
    groove_record = required(store, "kRecordBytes", CUSTOM_GROOVE_STORE)
    regions = [
        (at("kClockStateRegionOffset"),
         f"CURRENT and {presets} named presets ({current + presets * preset} B used: "
         f"{current} + {presets} × {preset})", "`persistent_layout::kClockStateRegionOffset`"),
        (at("kLegacyScoreRegionOffset"), "historical single-score records",
         "`persistent_layout::kLegacyScoreRegionOffset`"),
        (groove_offset, f"{groove_count} Custom Groove records of {groove_record} B",
         "`persistent_layout::kCustomGrooveRegionOffset`"),
        (at("kLeaderboardRegionOffset"), "four Top-100 leaderboard records",
         "`persistent_layout::kLeaderboardRegionOffset`"),
        (at("kSequencer2RegionOffset"), "Sequencer pattern bank", "`persistent_layout::kSequencer2RegionOffset`"),
        (at("kSequencerStepMetadataOffset"), "sparse Sequencer step-expression records",
         "`persistent_layout::kSequencerStepMetadataOffset`"),
        (at("kSequencerSongRegionOffset"), "reserved for Song Mode", "`persistent_layout::kSequencerSongRegionOffset`"),
    ]
    if groove_offset + groove_count * groove_record > at("kLeaderboardRegionOffset"):
        raise ValueError(f"{CUSTOM_GROOVE_STORE}: Custom Groove records overlap the leaderboard region")
    ends = [start for start, _, _ in regions[1:]] + [at("kCurrentImageBytes")]
    lines = ["| Bytes | Size (B) | Content | Defined by |", "| --- | ---: | --- | --- |"]
    for (start, content, source), stop in zip(regions, ends):
        if stop <= start:
            raise ValueError(f"persistence regions out of order at {source}")
        lines.append(f"| {start}–{stop - 1} | {stop - start} | {content} | {source} |")
    return "\n".join(lines)


def frequency(hz: int) -> str:
    """Format a frequency in the largest unit that keeps it an integer."""
    for unit, scale in (("MHz", 1_000_000), ("kHz", 1_000)):
        if hz % scale == 0:
            return f"{hz // scale} {unit}"
    return f"{hz} Hz"


def config_constants(root: Path) -> dict[str, int]:
    """Return the integer constants of src/config.h with the macro defaults resolved."""
    config = read(root, "src/config.h")
    macros: dict[str, int] = {}
    for name, value in defaults(config).items():
        try:
            macros[name] = evaluate(value, {})
        except ValueError:
            continue
    return integer_constants(config, macros)


def interrupt_priority_table(root: Path = ROOT) -> str:
    """Return the interrupt layer table with the preemption priorities from the code."""
    scheduler = required(config_constants(root), "kSchedulerInterruptPreemptPriority", "src/config.h")
    capture = required(integer_constants(read(root, EXTI_PRIORITY_SOURCE)), "kExtiPreemptPriority",
                       EXTI_PRIORITY_SOURCE)
    return "\n".join([
        "| Layer | Target priority | Responsibility |",
        "| --- | ---: | --- |",
        f"| TIM3 scheduler | {scheduler} | gate edges, pulse termination, phase, SYNC/RST consumption |",
        f"| GPIO EXTI | {capture} | conditioned SYNC/RST edge capture |",
        "| Foreground | n/a | UI, rendering, display service, persistence scheduling |",
    ])


def timing_constant_table(root: Path = ROOT) -> str:
    """Return the table of firmware timing constants from src/config.h."""
    constants = config_constants(root)

    def get(name: str) -> int:
        return required(constants, name, "src/config.h")

    tick = get("kSchedulerTickUs")
    catch_up = get("kMaximumSchedulerCatchUpTicks")
    rows = [
        ("Scheduler interrupt rate (TIM3)", frequency(get("kSchedulerFrequencyHz")), ["kSchedulerFrequencyHz"]),
        ("Scheduler quantum", f"{tick} µs", ["kSchedulerTickUs"]),
        ("Ticks caught up in one interrupt, at most", f"{catch_up} ({catch_up * tick} µs)",
         ["kMaximumSchedulerCatchUpTicks"]),
        ("Display refresh interval, at least", f"{get('kDisplayRefreshMinimumMs')} ms", ["kDisplayRefreshMinimumMs"]),
        ("Display SPI clock request (ceiling)", frequency(get("kDisplaySpiFrequencyHz")), ["kDisplaySpiFrequencyHz"]),
        ("Encoder long press", f"{get('kEncoderLongPressMs')} ms", ["kEncoderLongPressMs"]),
        ("TAP interval accepted", f"{get('kTapMinimumIntervalMs')}–{get('kTapMaximumIntervalMs')} ms",
         ["kTapMinimumIntervalMs", "kTapMaximumIntervalMs"]),
        ("Persistence quiet period after a change", f"{get('kPersistenceCommitDelayMs')} ms",
         ["kPersistenceCommitDelayMs"]),
        ("Persistence latency from the first unsaved change, at most",
         f"{get('kPersistenceMaximumLatencyMs')} ms", ["kPersistenceMaximumLatencyMs"]),
        ("Persistence retries after a failed commit",
         f"{get('kPersistenceRetryCount')}, first after {get('kPersistenceRetryInitialMs')} ms, doubling",
         ["kPersistenceRetryCount", "kPersistenceRetryInitialMs"]),
    ]
    lines = ["| Quantity | Value | Constant in `src/config.h` |", "| --- | --- | --- |"]
    lines += [f"| {quantity} | {value} | {', '.join(f'`{name}`' for name in names)} |"
              for quantity, value, names in rows]
    return "\n".join(lines)


def tables(root: Path = ROOT) -> list[Table]:
    """Return every generated table."""
    pin_map = read(root, "src/pin_map.h")
    config = read(root, "src/config.h")
    configuration = root / "docs" / "CONFIGURATION.md"
    timing = root / "docs" / "TIMING.md"
    return [
        Table("pins", configuration, pin_table(pin_map)),
        Table("display-macros", configuration, display_macro_table(pin_map, config)),
        Table("persistence-map", root / "docs" / "ARCHITECTURE.md", persistence_map(root)),
        Table("interrupt-priorities", timing, interrupt_priority_table(root)),
        Table("timing-constants", timing, timing_constant_table(root)),
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
