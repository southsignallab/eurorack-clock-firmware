<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# Sequencer 2.0 — contract

This document separates the Sequencer 2.0 scope released in 1.2.0 from later Sequencer work. The frozen 1.1.0 manual remains authoritative for 1.1.0 behavior; the frozen 1.2.0 manual and this file describe 1.2.0.

## Released in 1.2.0

- Maximum pattern length: **128 steps**. The OLED editor is an eight-step viewport, not a length limit.
- **8 pattern slots per channel × 8 physical channels = 64 pattern slots**. The active slot is persisted per channel.
- Binary gates use two explicit 64-bit words; no heap allocation and no compiler bitfields.
- Pattern-local traversal: `FORWARD`, `REVERSE`, `PINGPONG`, `RANDOM`. PINGPONG does not repeat the end points. RANDOM uses a deterministic, seedable, history-free mapping from event serial to step so rescheduling is reproducible.
- Pattern-local end behavior: `LOOP` or `ONCE`. RANDOM+ONCE emits exactly LENGTH traversal events; PINGPONG+ONCE emits one complete no-duplicate-endpoint traversal cycle.
- The production ClockEngine consumes the active SequencerPatternV2 snapshot for each channel. Legacy 1.1 Sequencer state remains only as the migration/fallback source when no V2 pattern has been activated.
- The Sequencer settings page places `EDITOR` first, followed by `PATTERN`, `LENGTH`, `ROTATE`, `MODE` (play direction), and `LOOP`.
- `EDITOR` opens the eight-step viewport directly. BACK returns to Sequencer parameters. Encoder long-press opens pattern operations; BACK from pattern operations returns to the editor.
- Turning the encoder moves the edit cursor continuously through the active pattern and changes the eight-step viewport when a boundary is crossed. The bottom page indicator follows this **edit viewport**, not the playback position.
- START/PAUSE controls transport while the editor stays open. The edit cursor remains a rectangle around the selected step; an independent small triangle at the bottom marks the currently playing step when it is visible in the current viewport.
- The editor uses a timing grid instead of per-step numbers: every four-step beat boundary is a longer/solid vertical reference and carries its beat number, while intermediate step boundaries use shorter ticks and dotted vertical lines. Gate cells are centered geometrically between the grid boundaries.
- Only traversal direction is duplicated in the live editor status as a compact forward, reverse, ping-pong or random pictogram. LOOP/ONCE remains editable in the Sequencer settings but is not repeated in the editor because normal looping does not need a second live-state symbol.
- Persistent pattern bank: fixed 2-KiB extension at logical offset 8192, explicit records + CRC, global rather than duplicated in CURRENT/eight presets.
- Legacy migration: each channel's released 1.1 64-bit Sequencer state becomes P1; P2–P8 keep clean defaults.
- Flash writes are coalesced by the central persistence coordinator (3-s quiet period, 30-s ceiling, one transaction with all other stores) and are not committed while transport is PLAYING.

## CURRENT step expression

Each step can carry sparse expression metadata without increasing the fixed pattern record or repeated `ClockState`:

- **Probability** — `DEFAULT` inherits channel probability; an explicit 1–100% value replaces it for that step. Non-selected overrides use a compact `%` marker. On the selected step, compact numeric digits replace that marker in the same grid position instead of moving the value into the header.
- **Gate / Duty** — `DEFAULT` inherits the channel gate length. Overrides provide short 1/2/5/10 ms triggers, relative 25/50/75% duty, or fixed 20/50/100 ms gates.
- **Tie** — holds a deterministic adjacent gate continuously rather than retriggering. The editor draws one continuous horizontal bridge from the current gate cell into the next gate cell. Tie is unavailable for RANDOM traversal and mutually exclusive with Ratchet.
- **Ratchet** — **1–8 total substeps**. `1` is the normal event and needs no symbol. Counts 2–4 use one row of dots; counts 5–8 use two rows with four dots on the first row and the remainder on the second. The Ratchet dots sit above the Probability marker/value so both attributes remain visually associated with the same step. Probability and Ratchet may be active on the same step.

Ratchets are bounded scheduler sub-events; they do not recurse and every emitted HIGH has a corresponding bounded LOW. Live pattern or metadata edits release an already-held Tie before replacing its runtime state, so editing cannot leave a gate stuck HIGH.

Per-step microtiming is intentionally **not** part of Sequencer expression. Deterministic timing displacement belongs to Swing/Groove. Humanize remains the separate One Clock stochastic layer rather than being duplicated as per-step metadata.

The sparse expression record is four bytes regardless of Ratchet count; the existing three-bit Ratchet field already represents 1–8, so allowing eight substeps does not increase persistent record size.

## PROPOSED Song Mode

Song Mode is a global topology peer to Independent, One Clock and Divider Bank, not a submenu of one channel. The arrangement grid has eight channel columns. Each row cell chooses `-` or P1–P8 for that channel. Row duration is defined on the master timeline rather than by waiting for all channel patterns to finish.

Polymeter therefore comes from different pattern lengths; polyrhythm comes from the existing rational per-channel rates. Pattern changes must remain anchored to the shared musical timeline. Exact CONTINUE/RESTART transition semantics still require a final contract before implementation.

## Not part of 1.2.0 (parked without a target version)

- Song Mode topology and arrangement persistence;
- USB full-device backup/restore transport and guarded device-side USB DATA mode.

## Persistence qualification boundary

The 12-KiB logical image consumes 4 KiB more static staging RAM than 1.1.0. Host migration and power-loss tests do not prove the target RAM budget. The release ELF passes the 90 % memory gate of `scripts/platformio_memory_gate.py`: Flash 116 212 of 229 376 B, RAM 37 836 of 65 536 B (`release_default`, 1.2.0).
