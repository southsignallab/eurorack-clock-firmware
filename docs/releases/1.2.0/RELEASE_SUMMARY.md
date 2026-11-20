<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# CLOCK 1.2.0

CLOCK 1.2.0 brings Sequencer 2.0 to the same STM32F401 hardware and deterministic master timeline, and closes the known timing, input, persistence and user-interface defects of the 1.1 line. Grooves, Pre-Count and the configurable inputs from 1.1.0 work as before.

## Highlights

- **Sequencer 2.0:** patterns of 1–128 steps, eight pattern slots P1–P8 per channel, `FORWARD / REVERSE / PINGPONG / RANDOM` traversal with `LOOP / ONCE`, and per-step Probability, Gate length, Tie and Ratchet 1–8. The editor shows eight steps at a time over the whole pattern with a live playhead.
- **Exact timing under live changes:** simultaneous gate edges leave in one register write, live edits no longer lose or double an edge, merged scheduler ticks are caught up, and tempo stays exact while it is being edited.
- **Reliable inputs and controls:** short triggers at INPUT 1/2 are counted, front-panel taps keep their press time, and the encoder keeps its detent after menu visits.
- **Safer start-up:** the gate outputs are configured LOW before anything else runs, and CLOCK no longer starts transport by itself at the first SYNC lock after power-up; the new `INPUT CONFIG` setting `AUTOSTART` brings that back on request.
- **Settings that survive:** one coordinated Flash commit path, no Flash writes while the engine plays, retries after a failed write with `SAVE ERROR` on screen, and an `UPDATE FIRMWARE` lock that keeps settings written by newer firmware intact after a downgrade.
- **Clearer menus and dialogs:** `APPLY TEMPLATE?` asks first with NO preselected, the display wakes from front-panel use and from START or STOP at an input, and lists, popovers and the Custom Groove choice behave consistently.
- **Qualification stays explicit:** 682 named Native tests with the 95/95/90 coverage gates intact. Physical timing measurements are tracked in the HIL ledger and are not replaced by host tests.

## Known issues

- **Hardware evidence follows in 1.2.1:** the timing and input fixes of this release are proven by host tests and calculation; the oscilloscope and logic-analyzer measurements on the module follow with 1.2.1.
- **Display idle timer (suspected):** after a long run that ends in STOP, for example after a lost SYNC lock, the display may dim or switch off sooner than the configured delay. This is under investigation.
- **Conservative display clock:** the display SPI runs at a calculated 656 kHz until the SSD1315 datasheet check; a faster clock is planned for 1.2.1.

## Firmware updates

Routine USB updates continue to use the persistence-preserving DfuSe release images, or the PlatformIO upload through `scripts/upload_preserving_persistence.py`; both leave the A/B persistence sectors untouched. Never use a mass erase or a contiguous `.bin` image across the persistence sectors. Disconnect Eurorack power before connecting USB. ST-LINK/SWD remains the recommended first-install and recovery path.

## Compatibility

CLOCK 1.2.0 keeps the Rev-1 GPIO and electrical hardware contract. Persistence advances to schema v13 and migrates the supported older formats, including 1.1.0 schema v12 and stable 1.0.x schema v8. The logical image grows from 8 KiB to 12 KiB for the Sequencer pattern bank; the first 8 KiB, with CURRENT state, named presets, Custom Grooves and score data, keep their layout and are promoted on the next save.
