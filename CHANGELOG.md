<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# Changelog

All notable prototype changes are documented here.

CLOCK follows semantic release milestones from the stable 1.0.0 baseline. The detailed history of the 0.x prototype and beta releases is archived unchanged in [`docs/releases/CHANGELOG-0.x.md`](docs/releases/CHANGELOG-0.x.md).



## [Unreleased]

### Added: proof that formatting leaves the firmware image unchanged

- `scripts/check_image_identity.py` builds the ten firmware environments (five target builds, five `release_*` flavors) into separate directories and compares the SHA-256 of `firmware.bin` and of every loaded ELF section with a reference manifest; it reports `equal` or `UNEQUAL` per environment. `CONTRIBUTING.md` describes the procedure.

### Documentation: one authoritative source per topic

- The User Guide chapter files under `docs/user-guide/` are authoritative. `docs/USER_GUIDE.md` is now an index of the 24 chapters; its operating content that existed only there (shared timing parameters, meter table, INFO page details, factory-reset details) moved into chapters 9 and 15.
- `docs/DOCUMENTATION_MAP.md` names the authoritative document for each topic and gives every documentation file one purpose.
- The pin and display-macro tables in `docs/CONFIGURATION.md` are generated from `src/pin_map.h` and `src/config.h` by `scripts/generate_reference_tables.py`.
- The 0.x prerelease history moved unchanged from `CHANGELOG.md` to `docs/releases/CHANGELOG-0.x.md`.

### Changed: documentation checks

- `scripts/check_documentation.py` validates the anchors of internal Markdown links against GitHub heading slugs, scans the VCV documents, requires `docs/USER_GUIDE.md` to stay an index, requires an inventory row for every Markdown document, and fails when a generated reference table is stale.
- The manual build and check name a PDF tool found first on `PATH` that is not the Poppler tool, such as the `pdftotext` of Git for Windows, instead of failing later with a decoding error.

### Documentation: corrections

- Menu paths and labels match the display: `SETTINGS → GENERAL SETTINGS → CLOCK → PRE COUNT`, `TEMPO`, `PROB`, `DIV/MULT` / `POLY N` / `POLY D`, `RST MODE`, `INT` / `EXT` for SOURCE and LOSS, `NAME` / `VERSION` on the INFO page.
- Features released in 1.2.0 (Sequencer 2.0, the 12 KiB persistence image with schema v13, the VCV Rack edition as an experimental part of the source tree) are no longer described as post-1.1 development; the target memory gate result is stated. Song Mode and USB backup are listed as parked without a target version.
- The display SPI is described with its 1 MHz ceiling and the calculated effective 656.25 kHz; the display-controller build flag is documented as currently without effect on the image.
- The broken anchor in user guide chapter 2 points to chapter 20; chapter 24 no longer presents I2C as a display option.
- Coverage and assertion figures are kept once, in `docs/TEST_COVERAGE.md` and the host test run, instead of repeating outdated copies in `README.md` and `test/README.md`; the input-suite case count in `docs/TEST_COVERAGE.md` is 125.

### Changed: HIL ledger state for tests that cannot apply

- `scripts/check_hil_qualification.py` accepts `NOT_APPLICABLE` with a mandatory, non-empty `reason` and no evidence; the all-PASS gate accepts it and the summary counts it separately. HIL-DISP-001, -003 and -004 are `NOT_APPLICABLE`: no firmware environment builds an I2C display profile, PB6/PB7 belong to the encoder.

### Changed: release summary

- The release summary has a `## Known issues` section from 1.2.0 on, with at most eight items or a sentence that none are known; `scripts/check_release_summary.py` counts these bullets separately and `scripts/prepare_release_summary.py` creates the section. The 1.2.0 summary lists three known issues.

### Documentation: one source per topic, generated tables, audits

- Persistence is described once, in `docs/ARCHITECTURE.md`: a logical image map generated from `src/hal/persistent_layout.h` and the stores, the stored state, and the schema versions with the lifetime of each decoder (prerelease formats until 1.2.x, v8 and v12 until 2.0.0). `docs/CONFIGURATION.md` and `docs/V1_FORWARD_COMPATIBILITY.md` link to it.
- The interrupt priorities and a table of timing constants in `docs/TIMING.md` are generated from the code; `scripts/generate_reference_tables.py --check` covers them.
- Coverage gates are kept in `docs/TEST_COVERAGE.md` and the release preparation and tag-build steps in `docs/RELEASE_PROCESS.md`; `docs/DEVELOPMENT.md` and `docs/DEVELOPER_README.md` link to them.
- User guide checked against the menu model, defaults, timing and pin map: One Clock lists GROOVE, the Groove editor zoom steps, the `INTEGER` divider family, the conditional `HI-SCORES` root row, the `FONT` license rows, `DIV/MULT` in the recipes and the Sequencer recipe order are corrected; `README.md` lists `AUTOSTART`; the legacy I2C transport is described as compiled for the target but not linked into any image.

### Changed: code formatting

- `.clang-format` matches the house style (case labels indented, one argument or parameter per line when a call breaks, two spaces before trailing comments, no comment reflow) and names the pinned version, clang-format 23.1.5; `sim/tutorial/.clang-format` packs call arguments. `CONTRIBUTING.md` names the version.
- `src/`, `lib/`, `sim/` and `vcv/` are formatted with it, except thirteen implementation files that would exceed the 450-line limit and still need to be split. Bodies of control statements in the HAL, the games and the screensavers have braces. All ten firmware images are byte-identical to the previous commit.
- `scripts/check_architecture.py` skips the message of a `static_assert` that is wrapped over several lines when it looks for UI text outside `src/ui_text.h`.

### Changed: code clean-up without behavior change

- Bitwise operations in the engine, the HAL, the stores and the screensavers use unsigned operands of fixed width; no operand is a promoted signed `int` any more.
- Quantities defined more than once have one definition: the exit long-press time of the boot games (`src/game/boot_game_controls.h`), the OLED page width, the Egg Journey ground, egg height and physics scale, the editor beat-label row and the dialog choice columns.
- The Custom Groove region is defined in `src/hal/persistent_layout.h` like every other persistence region, with compile-time order checks; the layout and its values are unchanged.
- The factory groove names `OFF` … `CUSTOM` are defined in `src/ui_text.h`; the texts are unchanged, and the architecture check now covers the groove catalog.
- Every discarded result states its reason; an unused test accessor and redundant casts are removed; deliberately uninitialized scheduler records and defensive initial values in the tick path are marked with the reason.
- The Groove editor cursor wraps through `UiController::wrapIndex()`; `scripts/check_architecture.py` rejects an inline modulo on a cursor index (the F-004 error class).
- `src/config.h` stops an STM32 build that sets `CLOCK_HOST_TEST` or `CLOCK_SIMULATOR`, and the architecture check allows Arduino-style framework includes in `src/` only in the host-test and simulator branch.
- The gate-buffer comments in `src/pin_map.h` no longer name buffer input positions that differ between hardware variants.
- `BUILD-INFO.txt` hashes the linker map with the build and package directories replaced by `<build>` and `<packages>`, so the same source gives the same file in any build directory.
- `.clang-tidy` is the proposed project profile with a reason per check; `scripts/check_static_analysis.py` compares the findings with `scripts/static_analysis_baseline.json` and fails on new ones. It is not yet part of CI.

### Changed: code documentation tooling

- The Doxyfile defines `@license`, which removes 209 Doxygen warnings.
- `scripts/check_doxygen.py` counts Doxygen warnings per source directory and fails when a directory exceeds `scripts/doxygen_warning_baseline.json`; a new undocumented declaration, including a public struct field, therefore fails it.
- CI uploads the generated Doxygen HTML as a workflow artifact.
- `CONTRIBUTING.md` holds the rules for code documentation: what is documented, units, interrupt-context contracts and the warning baseline.

## [1.2.0] - YYYY-MM-DD

### Changed: repository in the citation metadata, CI step names

- `CITATION.cff` (`repository-code`) and `codemeta.json` (`codeRepository`) name https://github.com/southsignallab/eurorack-clock-firmware; the documentation check requires it.
- The firmware build steps in the CI workflow are named after the PlatformIO environments they build (`blackpill_f401cc`, `blackpill_f401cc_spi_ssd1315`).

### Documentation: SAVE ERROR and UPDATE FIRMWARE in the manual

- The storage chapter of the user manual explains SAVE ERROR (seven attempts, then a pause until the next change or power-up, shown for 3 seconds after every front-panel action) and UPDATE FIRMWARE (only FACTORY RESET lifts the lock), as the user guide does.

### Changed: native tests are as strict as the host coverage run

- `pio test -e native` builds the firmware sources and the test suites with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror`, the flags of `scripts/run_host_tests.py`. A `test/unity_config.h` keeps PlatformIO from generating its own Unity output adapter, which does not build with these flags.

### Fix: VCV Rack keyboard holds and coverage check on Windows

- A control held through the keyboard in VCV Rack (Shift, Enter/E, P, T, S/Backspace) is released when the pointer leaves the module and when its key is released while the pointer is elsewhere. Before, such a release did not reach the module and the control stayed held.
- `scripts/check_vcv_runtime_coverage.py` runs on Windows with MinGW-w64 GCC and Ninja from `PATH` instead of picking MSVC; without them it stops with exit code 2 and names the missing tools.
- The Custom Groove store checks the slot index inside its cache helper as well, which removes a false GCC 16 `-Warray-bounds` warning in the optimized VCV build.

### Fix: safe start-up and output stage

- The eight gate outputs are configured and driven LOW as the very first step of start-up, before the stored settings are loaded. Each output's level is set before the pin becomes an output, so it never drives HIGH for an instant.
- Enabling the gate output stage sets the engine's gate state to LOW in the same step, so engine and outputs agree from the first event. Before, a gate the engine raised while the stage was still muted stayed recorded as HIGH although the output was LOW.
- After a reset the bar/beat counter that follows an external clock starts again from bar 1; it no longer jumps back to the position before the reset with the next clock pulse.
- A display page whose SPI transfer fails is sent again with the next frame instead of staying wrong until its content changes.

### Fix: boot games

- Pixel Raid ends with GAME OVER and records the high score when the invaders reach the cannon line; before, the formation sank without limit.
- BEATKNECHT schedules each step from its target time and carries the remainder of the step length, so the rhythm no longer drifts (for example 112.78 ms steps at 133 BPM).
- The long encoder hold that leaves a game is recognized when the press falls exactly on millisecond 0 of the system time (once every 49.7 days), and the Formula 1 physics no longer treats a frame at that time as the first frame.
- The Pixel Raid life-lost blink writes all eight muted gate sources in one register access per update.

### Changed: checked results of storage, commit and display transfers

- Storage, commit and display-transport functions are marked `[[nodiscard]]`; every call either uses the result or states why it is not needed.

### Fix: display protection with a running external clock

- A clock at SYNC no longer keeps the display awake. Only front-panel controls and START, STOP, RESTART or RUN commands at an input count as activity; SYNC pulses, RESET triggers, TAP at an input and inputs set to OFF do not. Screensaver, dimming and OLED off now also start in STOP while a master clock keeps running.
- While the OLED is switched off, turning or pushing the encoder, TAP TEMPO and BACK in a menu only wake the display; the release of that press is ignored too. START/PAUSE and STOP act at once and wake the display, and so does a START or STOP command at an input. Previously a turn changed the tempo or a value invisibly.

### Fix: menus and editors

- Factory templates ask `APPLY TEMPLATE?` with NO preselected before they replace the configuration; NO and BACK return to the list unchanged.
- The template list keeps the cursor on a visible row. The list scrolled after five rows but showed four, so templates 5 and 6 could be applied without being visible.
- Groove Record aligns a recording started in the middle of a step with the grid of the channel that plays the Groove: capture begins at the next step and numbers the steps as playback does. Previously every recorded offset carried the start phase, and the steps were shifted.
- Releasing TAP after holding it as a modifier while turning the encoder no longer also triggers the TAP action (Tap Tempo, Groove Editor cursor, Sequencer page).
- The Performance screen shows the Sequencer length of the active pattern; a LENGTH change in Sequencer 2.0 was not shown.
- After PASTE of a shorter Sequencer pattern the editor cursor moves to the last step instead of staying outside the pattern.
- Groove PRESET offers `CUSTOM` only while a Custom Groove slot holds a groove; selecting it uses the channel's slot or, if that slot is empty, the first stored groove.
- After the HI-SCORES CLEAR dialog the cursor returns to HI-SCORES instead of INFO.
- A PLAY press used for zoom in the Groove Editor no longer swallows the next PLAY in the recorder after a screen change.
- The information popover wraps at spaces and after `/`, so `ko-fi.com/southsignallab` is no longer broken inside a word; only a word longer than one line breaks inside the word.

### Changed: documented test counts are checked

- `scripts/check_test_inventory.py` now compares the per-suite and total Native test counts in `test/README.md`, `README.md` and `docs/DEVELOPMENT.md` with the `RUN_TEST` registrations and fails on any difference. The three documents listed the counts of an older state (525 instead of 655 cases).

### Fix: short triggers at INPUT 1/2, encoder after menu visits

- A trigger at INPUT 1 or INPUT 2 that is already over when the input interrupt runs now fires its role. START, STOP, RESTART, TAP and RESET (`TRIGGER`) fire exactly once; RUN and RESET (`GATE`) follow only the level. Previously such a trigger was read as LOW and lost, for example a START trigger while a preset was written to Flash with the clock stopped; it now arrives after the write. Not yet measured on hardware.
- An edge that arrived while the input interrupt read the pin is no longer counted twice.
- The input edge queue publishes each edge with release/acquire ordering, so the scheduler cannot read an edge before it is complete.
- The encoder no longer loses a detent at each direction change after a menu visit. The detent state now comes from the encoder datasheet (A and B LOW), and releasing the encoder button only drops a partial count once the encoder has rested 3 ms in a detent. Previously the A/B state at the release became the new detent phase, even mid-turn.
- Transport commands from the inputs (START, STOP, RESTART, RUN, auto-start on lock, stop on sync loss) no longer disable interrupts inside the scheduler interrupt, and a START deferred by a Flash write is prepared outside the interrupt lock. `scripts/check_architecture.py` reports interrupt code that opens an interrupt lock.
- The shared interrupt handlers for EXTI lines 5–9 and 10–15 iterate over line numbers; the 10–15 handler no longer loops forever once line 15 is pending (no line 10–15 is used today).
- The debug counters in `gClockRuntimeCounters` also count synthesized input edges, encoder anchors, the A/B state at the last anchor request and discarded partial encoder counts.

### Fix: front-panel taps keep their press time

- TAP TEMPO and Groove Record now time a front-panel press from its first contact edge instead of from the debounced edge. Debouncing (25 ms) still decides whether a press counts. Previously a Groove Record tap was late by the contact bounce plus up to one control pass, and TAP TEMPO measured from release to release, so holding TAP longer or shorter for one tap changed the tempo. The same applies to every front-panel button and the encoder push.
- A Groove Record tap whose contact edge falls exactly on microsecond 0 of the timer (once every 71.6 minutes) keeps its time. Previously it fell back to the debounced time, about 25 ms later.

### Fix: short interrupt locks for every live change

- Custom Groove library and preview changes, Sequencer pattern changes and PLAY, STOP and RESET now prepare the channel schedules outside the interrupt lock, as configuration changes already did; the lock only exchanges pointers and installs prepared values. Previously up to eight channels were recomputed with interrupts disabled. Not yet measured on hardware (`blackpill_f401cc_timing`).
- Custom Groove patterns and the Sequencer step expressions are no longer copied byte by byte with interrupts disabled (65 and 256 bytes); a spare buffer is filled first and swapped in.
- STOP and RESET lower all HIGH gates with one register access instead of one per output.
- The debug counters in `gClockRuntimeCounters` also count staged changes that had to compute under the lock (`stagedSwapFallbacks`).

### Fix: exact tempo while editing, one time base

- The master timeline no longer loses scheduler ticks while the foreground holds an interrupt lock. Each scheduler interrupt counts the ticks since the previous one from the microsecond timer (TIM5) and catches up up to 8 ticks (400 µs) at once; events that fall into those ticks fire once each and in order. Previously several 50-µs ticks merged into one, so the clock ran slightly slow while settings were edited during PLAY (not measured).
- Configuration changes during PLAY compute the new channel schedules outside the interrupt lock; the lock covers only the swap of the prepared values. If the scheduler moves a schedule meanwhile, the preparation repeats, and after three attempts the swap computes under the lock as before. Previously up to eight channels were recomputed with interrupts disabled. Not yet measured on hardware (`blackpill_f401cc_timing`).
- Milliseconds now derive from the same TIM5 time base as microseconds, so they keep counting through Flash operations, which stall the SysTick counter. Millisecond timers (screen timeouts, button timing) no longer drift from the microsecond input timestamps. SysTick remains for HAL-internal timeouts.
- A control pass whose microsecond timestamp is exactly zero (once every 71.6 minutes) no longer falls back to milliseconds × 1000.
- After an early SYNC pulse, the following beats stay on the pulses. Previously an early correction larger than the time since the phase was set (typically right after the first pulses) moved every following beat late by the excess until the next reset; in the reproduced case 10 ms. An early pulse now moves the master timeline forward instead of moving the beat grid back.

### Fix: simultaneous gate edges, no lost or doubled edges on live changes

- All gate edges of one scheduler tick (50 µs) are written to the outputs in one register access, so outputs that rise or fall together switch at the same instant. Previously each output was written separately with engine work in between, so later outputs switched slightly after earlier ones (not measured). BEATKNECHT raises the drum gates of a step the same way.
- A pulse is now followed by at least one tick LOW before the next rising edge. When the next event moves closer while a gate is still HIGH (tempo jump, live Groove or Swing change, Ratchet), the gate ends one tick early and the rising edge stays on time; only when a change moves the next event into the very next tick (for example a reset while a gate is HIGH) does the rising edge move by one tick. Previously the rising edge was lost and two clocks merged into one gate. Tie still holds the gate HIGH across steps.
- A live change of Custom Groove (slot or preview), Groove preset, rotation, Swing or phase no longer plays the last event a second time when its new position lies after the current time. Rate, mode and pattern changes, tempo changes and SYNC corrections are unaffected; a reset still restarts the bar on purpose.
- At the scheduler's resolution limit (events less than two ticks apart, for example the shortest Swing intervals at the highest rates) such events can share one pulse; the former LOW/HIGH changes within one tick no longer occur.

### Added: runtime instrumentation in debug builds

- New PlatformIO environments `blackpill_f401cc_debug_instrumented` and `blackpill_f401cc_timing` (release optimization `-Os`) record the longest TIM3 scheduler interrupt, the longest foreground interrupt lock (CPU cycles), the number of moved rising edges, caught-up scheduler ticks and interrupts that reached the catch-up limit in `gClockRuntimeCounters`, readable over SWD. Release builds contain none of it.

### Changed: one coordinated Flash commit path

- All settings stores (CURRENT, presets, Custom Grooves, Sequencer patterns and step expression) now commit through one coordinator in **one** Flash transaction, so one sector erase covers every pending change instead of up to four.
- Ordinary edits are written 3 s after the **last** change, but no later than 30 s after the first unsaved change. Previously the Sequencer banks committed 3 s after the first change even while editing continued, which erased a sector about every 3 s during long edits. Explicit saves (presets, Custom Grooves) are still written at the next opportunity.
- A failed commit is no longer repeated in every foreground pass. Retries follow after 1, 2, 4, 8, 16 and 32 s; the Performance screen shows **SAVE ERROR** after each failure. Pending changes stay in memory.
- After the last retry fails, CLOCK stops writing until the stored data changes again (settings, presets, Custom Grooves or Sequencer patterns) or the module restarts, so a worn sector is not erased indefinitely; until then **SAVE ERROR** appears for 3 s after every front-panel action. The next change starts a new series with the normal quiet period. Previously the retries continued every 60 s.
- Immediately before a sector erase, the scheduler interlock checks the engine and opens a commit window under one interrupt lock. A RUN/START/RESTART edge or an auto-start on lock inside that window starts transport right after the commit instead of during it.

### Changed: faster boot

- Both Flash slots are validated once per boot; the result is cached until the next commit, which re-validates only the slot it wrote. All persistent records use one table-driven CRC-32 that is bit-identical to the previous bitwise implementation. Previously every record read recomputed the CRC over both 12-KiB slots (22–94 times before the first image; estimated up to about 2.5 s). The 1-s boot screen is unchanged. Not yet measured on hardware.
- Custom Groove names shown on the Performance screen are served from RAM instead of being read from Flash in every frame.

### Fix: downgrade no longer overwrites newer settings

- A Flash slot with an intact header in an unknown format or size (written by newer firmware) was treated as empty, so the first autosave after a downgrade overwrote it. Such a slot now blocks every write while it holds the newest generation; the Performance screen shows **UPDATE FIRMWARE**, and the firmware keeps running with the newest settings it can read. Only the confirmed **FACTORY RESET** replaces it.

### Fix: factory reset clears every store

- **FACTORY RESET** now also discards queued Custom Groove writes, both Sequencer banks in RAM and the engine's Groove and Sequencer copies. Previously an edited pattern kept playing and was written back to Flash a few seconds after the reset.

### Changed: no autostart after power-up; new SYNC setting AUTOSTART

- CLOCK no longer starts transport on its own when the first SYNC lock after power-up is acquired. New `INPUT CONFIG` row **AUTOSTART** (`OFF / ON`, factory `OFF`): with `ON`, a lock may start transport, including the first one after power-up. Switching it while stopped applies to the next lock.
- Persistence schema **v13** appends the setting to the CURRENT and preset records (payload 295 bytes, CURRENT 307, preset 323; repeated-record headroom now 20 bytes). v12 records from firmware 1.1.0 migrate with `AUTOSTART = OFF`. Firmware 1.1.0 cannot read v13 records; a downgrade falls back to defaults.

### Changed: tempo ownership with an external clock

- `SOURCE = AUTO` without a signal: a manual tempo change (encoder, TAP, settings) replaces a held freewheel tempo; a returning signal takes over again. Previously the last measured tempo stayed in effect until restart while the display followed the manual value.
- `SOURCE = EXTERNAL`, or `AUTO` with a locked signal: the Performance encoder and TAP no longer change the tempo; the display shows **EXTERNAL CLOCK** for 1.5 s. Menus and settings stay fully usable. A TAP input role is ignored in this state.

### Changed: front-panel transport wins until the next RUN edge

- With an input assigned to RUN, a front-panel STOP/PAUSE (RUN HIGH) or START (RUN LOW) now holds until the RUN level changes the next time. Previously RUN HIGH restarted the clock at the next scheduler tick after a manual STOP.

### Fix: SYNC pulses while stopped no longer shift the grid

- Pulses received during STOP/PAUSE moved every GLOBAL channel schedule one pulse interval into the past; PLAY then caught up as a burst and Euclid/Sequencer did not start on step 1. Phase correction now runs only while playing, a fresh start without Pre-Count begins at step 1 and beat 1, and pending ratchet sub-events are shifted together with their step.

### Fix: failed boot migration commit is retried, not ignored

- When the commit of a schema migration failed at boot, the next autosave wrote current-schema records over the older layout and could destroy presets. The migration is now retried (with the commit coordinator's backoff) inside the same transaction as, and before, any other write of the settings store; presets are served from the older layout until then.
- Stale STM32 Flash error flags are cleared before every sector erase (a stale flag made the first commit after reset fail).

### Fix: persistence-preserving DFU upload recognizes current slots (#12)

- Validate both A/B slots against the firmware's slot contract for every accepted payload size (4, 8 and 12 KiB). Previously only 4-KiB slots were recognized, so current 12-KiB slots always looked invalid and the uploader fell into the legacy-migration path.
- Read sectors 1/2 in full before every upload and keep the readback as a timestamped backup in `.pio/persistence-backups/`; the log reports generation and payload size of each valid slot.
- Run the pre-A/B STM32duino EEPROM migration only with `--migrate-legacy-eeprom`, because sector 5 now holds application code. PlatformIO forwards the switch through `upload_flags` / `PLATFORMIO_UPLOAD_FLAGS` (`$UPLOAD_FLAGS` in `upload_command`).
- Tests check the uploader constants against `persistent_storage.cpp`, `persistent_storage.h` and `persistent_layout.h`.

### Fix: cyclic cursors stay in range for any encoder delta

- Channel overview and mode carousel wrapped with a negative C++ remainder; a large backward delta (up to −127 detents in one sample after a stalled foreground pass) produced cursor 249 and indexed past `channels[8]` / `kModeFunctions`. All three wrap sites now use `UiController::wrapIndex()`.
- Defensive bounds: channel selection from the overview only accepts indices below eight, mode commit rejects an out-of-range carousel index, and the Groove settings page ignores an out-of-range channel.

### Fix: no Flash commit while the engine runs; stale ISR transitions ignored

- The commit gate asks the engine (`ClockEngine::isPlaying()`, read under an interrupt lock) immediately before each store, not only the foreground transport field. The scheduler ISR can start the engine mid-pass (RUN level, START/RESTART input, lock acquisition); a sector commit then stalled a running engine.
- A queued ISR transport transition is adopted only while it still matches the engine, so a front-panel STOP that overtook a RUN start no longer leaves the display on PLAY with a stopped engine.

### Fix: Humanize keeps close Custom Groove events apart (#6)

- ONE CLOCK Humanize derived its safe range from the factory groove table only, so signed Custom Groove offsets were ignored. With a tight Custom Groove (for example `[-120, +120]` at 100 %, Swing 50, at fast rates) two adjacent events could swap order or collapse into one gate. The range now follows the smallest event spacing of the active Custom Groove or editor preview, including Rotation, Amount and the Swing clamp; it is computed when the pattern or configuration changes, never in the scheduler tick.
- The case reported in #6 (120 BPM, CLOCK ×1, Gate 100 ms, Custom Groove `[-120, +120]` at 100 %) already produced two rising edges; the gate is limited by the actual distance to the next event. A regression test now checks the reported edge times.

### Fix: external TAP after 71.6 minutes (#8)

- Tap Tempo now times panel and input taps in one microsecond time base with unsigned differences. Previously an input tap's microsecond timestamp, which wraps after about 71.6 minutes, was compared with milliseconds; afterwards external TAP pulses no longer set a tempo, and mixed panel and input taps produced wrong intervals.

### Fix: VCV Rack port and simulator (#9, #10, #11, #13)

- VCV: saving a patch stores the live settings, including changes that the firmware has accepted but not yet committed (commits wait for STOP). Previously a patch saved while playing, or right after an edit, could restore older settings.
- VCV: the persistence mirror file is written by a worker thread; Rack's audio thread performs no file operations.
- VCV: releasing T, Enter/E, P, S or Backspace while Ctrl, Alt or Super is held now releases the virtual control. Previously TAP, the encoder push, PLAY or STOP could stay held.
- Simulator: time advances shorter than one 50-µs scheduler tick are carried into the next advance. Previously the remainder was dropped, so wall/input time and engine time drifted apart (1000 × 25 µs fired no scheduler tick).

### Build: SSD1315 is the default display profile

- `default_envs`, `release_default` and all `release_*` Easter-egg variants build `blackpill_f401cc_spi_ssd1315`, matching the reference board's Waveshare 24102 OLED module. `blackpill_f401cc_spi_ssd1306` and the `blackpill_f401cc_spi` alias remain available.

### Hardware alignment: KiCad front panel as the only panel source

- Add `scripts/import_kicad_front_panel.py`. It reads `Simple Frontpanel.kicad_pcb` (Edge.Cuts and F.SilkS) without a KiCad installation and writes the canonical panel artwork `docs/assets/front-panel/clock-front-panel.svg`, a 20 px/mm PNG for web/print, the simulator panel image `sim/panel_background.png`, and the panel size and every control coordinate in `sim/panel_layout.ini`. `--check` verifies the committed files against the KiCad file.
- Build the manual front-panel illustration and the VCV Rack panel from that artwork: callouts and Rack widgets sit on top of the real engraving. The VCV lettering overlay is removed; Rack tooltips use the engraved names.
- The simulator uses the KiCad engraving as `background_image` and no longer draws its own debug labels by default.
- Move every simulator/VCV coordinate to the hardware Rev C panel: display window 25.0 × 13.6 mm, DATA/ENTER encoder, START/PAUSE / TAP TEMPO/SHIFT / STOP/BACK buttons, IN 1 / IN 2, the 2 × 4 output matrix with LEDs above and right of each jack, and the four mounting-slot centres. Update the built-in defaults and the layout contract test accordingly.
- Draw the VCV OLED at a uniform scale centred in the (not exactly 2:1) panel window so pixels stay square.
- Replace stale PEC11L-4120K-S0020 (20 detents) encoder references with the fitted Bourns PEC11R-4220F-S0024 (24 detents / 24 PPR). Detent decoding is unchanged: both parts produce one quadrature cycle per detent.
- Correct `pin_map.h` comments: gate sources feed the 74HCT541 only and the LEDs hang on the buffered outputs; button names follow the schematic; input jacks have no jack-detect path (TN tied to GND).
- Bring the README hardware list in line with Rev C and note the missing jack-detect path in HIL-SYNC-006.
- Name the physical controls as engraved throughout the Markdown documentation: START/PAUSE, TAP TEMPO/SHIFT, STOP/BACK, DATA/ENTER, IN 1, IN 2. Display literals (`PLAY`, `STOP`), transport states, input roles (`SYNC`, `TAP`, …) and feature names such as TAP Record keep their names.

### Build: Windows VSCodium / PowerShell simulator toolchain

- Keep native simulator CMake presets on MSYS2 UCRT64 GCC under Windows instead of inheriting legacy Visual Studio 2013 from the host environment; leave Linux/macOS compiler selection unchanged.
- Add a PowerShell-friendly Python entrypoint that validates a C++17 compile and launch, detects stale compiler/generator caches, and reconfigures/rebuilds using Ninja.
- Route VSCodium simulator tasks and headless simulator tests through the same bootstrap path; document the Windows-first VSCodium/PlatformIO/CMake workflow and add cache-repair regression tests.


### Fix: preserve multiple deferred preset saves (#7)

- Track pending Save, Rename, and Clear operations independently for all eight preset slots, rather than silently replacing the previous slot's pending record.
- Stage all accepted mutations and any pending CURRENT state in one A/B Flash transaction once transport permits writes.
- Preserve queued changes after a failed commit; expose the pending state consistently through preset loading, names, and availability.
- Add regression coverage for concurrent deferred slots during PLAY, failed-commit retry, reboot, same-slot replacement, and factory reset cancellation.

### Regression: Custom Groove close-event gate edges (#6)

- Assert real rising/falling GPIO edges for the reviewed 120 BPM / 100 ms gate / -120,+120 Custom Groove scenario across Swing, rotation, and live preview.
- The current event-to-next-event gate limiter already keeps the reported rising edges distinct; do not change production timing semantics without a failing current vector.


### Fix: safe Custom Groove persistence during transport (#5)

- Queue Custom Groove save, rename and delete mutations in bounded RAM while transport is PLAYING; do not erase/program MCU Flash from those UI handlers.
- Commit all queued Groove records in one A/B storage update when PAUSED/STOPPED, before committing CURRENT state references to the same slots.
- Keep uncommitted Groove changes visible to the editor and retry a failed Flash transaction without discarding accepted changes.
- Add native UI save/rename/delete regression coverage and headless storage-transaction regressions.


### Storybook publication assets, narration and repository hygiene

- Expand the editable teaching set from thirteen to fourteen examples with **External Sync, Reset & General Settings**, a scenario-driven walkthrough that atomically demonstrates Settings entry, configurable input roles, AUTO lock acquisition, PPQN/EDGE, all three loss policies, TRIGGER/GATE reset semantics, RUN/START/STOP/RESTART/TAP transport roles, FILTER/SMOOTHING/TIMEOUT, Diagnostics, Mode & Clock, Screensaver and Hardware preferences.
- Expand the editable teaching set from twelve to thirteen examples with **Getting to Know CLOCK**, a standalone general tour that explains the project motivation and current hardware-verification status, then demonstrates One Clock, Divider Bank, Independent channel switching, Euclid and Sequencer without turning the overview into a detailed operating tutorial.
- Expand the editable teaching set from eleven to twelve examples with a standalone Groove Editor Walkthrough that boots from factory state, references Eight Independent Clocks Walkthrough by name, enters channel-four Groove settings through the real UI, and demonstrates editor anatomy, fine/coarse offsets, TAP step advance, PLAY-modified zoom, long-press context actions, pattern length, Custom Groove save/activation, Amount/Rotate and dirty-draft discard protection.
- Add the approved South Signal Lab 1080p intro and outro as the only intentionally tracked Storybook MP4 source assets, and reference both from every editable teaching example.
- Ignore generated tutorial videos, audio renders, subtitle sidecars, retained frame trees, staging directories and Storybook frame PNG/RGBA intermediates so publication output cannot drift into source control.
- Keep the editable tutorial series name-based in all viewer-facing titles, cards and narration; numeric filename prefixes remain only an internal ordering convention. Rework the US-English ElevenLabs narration around the full South Signal Lab opening/sign-off contract, explicit viewer orientation before detailed operation, and purposeful Audio Tags for warmer, more natural delivery.
- Recalculate every editable example from the actual narration length at the measured ~195 wpm reference pace; retain 18-second opening and 20-second closing windows while substantially extending body holds where the explanation needs room.
- Expand the editable teaching set from ten to eleven examples with a full eight-Independent-Clocks walkthrough covering product orientation, topology change, short-push channel selection, the real encoder long-press, and a separately timed explanation of every MODE / TIMING / CLOCK / OUTPUT setting actually shown on screen.
- Keep subtitles concise while narration beats explain intent, context and parameter meaning in more detail.
- Tighten Groove Editor Walkthrough pacing without shortening its spoken content: split the narration into smaller action-aligned TTS segments, use a 400-ms per-story body headroom, and keep checked-in beat durations as physical-action minima rather than speaker-time estimates.
- Align Groove tutorial focus rectangles to the actual 128x64 Groove/settings renderer rows and status regions instead of approximate full-screen boxes.
- Make the scope demonstration visually explicit by editing the saved Custom Groove to -28/256 on step 1 and +80/256 on step 2 before applying Amount 80% and Rotate 1.
- Add optional `encoder.speed: fast` Story actions for deliberate long tutorial sweeps while preserving the same detent count and resulting product state.
- Add a local narration-first publication orchestrator: synthesize/cache ElevenLabs segments, measure real audio duration, resolve Story timing from those measurements, render the picture only afterwards, and mux narration against Storybook-authored cue positions.
- Make ElevenLabs model selection local and runtime-resolved through `/v1/models`, preferring an accessible v4 TTS model via `ELEVENLABS_MODEL_ID=auto-v4` without hard-coding a provider model identifier.
- Add a tracked `.env.example` while hard-ignoring real `.env`/`.env.*` files; API keys remain workstation-only and are never copied into manifests or render reports.
- Add sparse publication-frame storage and FFconcat run-length encoding so unchanged tutorial frames reuse one PNG instead of losslessly encoding thousands of duplicates; logical output remains CFR at the Story FPS.

### Storybook focus-arrow readability

- Replace hand-maintained tutorial OLED rectangles with canonical semantic focus regions derived from the production 128x64 renderers for top bar, tempo, channel grid, settings rows, Divider Bank, Euclid/Sequencer feedback, Groove editor/status/library and Sequencer editor geometry.
- Clamp focus strokes to the actual OLED raster at display edges so full-width/full-height targets no longer draw their highlight outside the display area.
- Shorten the end of **Getting to Know CLOCK** by removing the repeated mode/feature recap and second motivation summary; after the deeper-walkthrough pointer it now goes directly to the project-follow/sign-off block.
- Reworked tutorial focus arrows into short collision-aware leaders that terminate outside the highlighted control/OLED region instead of crossing it.
- Added adaptive auto-placement plus optional `focus.placement: left|right|above|below` authoring control.
- Added a three-layer black/white/CLOCK-blue leader and arrowhead so callouts remain visible across bright OLED pixels, dark cards and panel artwork.
- Kept focus geometry presentation-only and derived from the existing PanelLayout/canonical OLED coordinates.

### Changed

- Refined Storybook publication visuals: Ubuntu/Ubuntu Mono are now the configurable default publication fonts, chapter/interstitial screens use the manual CLOCK Blue `#0B4FC0`, normal tutorial views default to black, and OLED/panel content sits on a cleaner dark card layout with a dedicated subtitle bar.
- Added optional theme-driven tutorial background images with `cover` / `contain` / `stretch`; the SDL-free renderer accepts uncompressed 24/32-bit BMP without introducing an image-codec dependency.
- Slowed `HUMAN_NORMAL` and `HUMAN_SLOW` substantially for instructional legibility while preserving `HUMAN_FAST` for semantically timed interactions such as Tap Tempo.
- Added a separate `south-signal-lab-ci` theme so renderer CI remains dependency-free and deterministic while publication typography uses installed system fonts.


### Fixed

- External Sync, Reset & General Settings now advances the real one-second boot lifecycle before the first `TAP + encoder push` Settings chord; opening chapter/text cards intentionally freeze firmware time, so the previous story could issue the correct gesture before Performance was active.
- Added a Storybook regression that executes the real `TAP down → encoder push → TAP up` gesture after boot, then confirms the default root selection by opening `GENERAL SETTINGS` through the production UI.

- Fix the External Sync, Reset & General Settings narration catalog using an invalid `s26-stop` segment identifier: narration IDs are now a continuous `s01` through `s43`, and repository tooling validates the same numeric-ID contract enforced by the local renderer.
- Fix narration-resolved tutorial publication losing its intro/outro asset base directory: generated stories now rebase any relative `intro_video` / `outro_video` reference against the authored Story location, serialize an absolute normalized path under `tutorial-output/resolved-stories/`, and fail before frame rendering if the resolved publication asset does not exist.
- Fix Windows Storybook rerenders failing with `Permission denied` while replacing `<output>.frames`: video publication now renders into a unique run-scoped frame directory, so stale Explorer/Defender/indexer handles on a previous frame tree cannot block the next run; temporary run trees are cleaned after publication unless `--keep-frames` is requested.
- Fix tutorial narration synchronization by replacing action-local narration holds with explicit `beat` / `beat_end` containers: voice-over now begins before the UI actions it describes, those actions consume the same resolved audio budget, and only the remaining tail is held afterwards.
- Reject the legacy tutorial forms that could speak after an operation had already completed, preserve zero-duration final focus through the active beat, and fail explicitly if enclosed actions exceed the resolved narration budget.
- Preserve deliberate authored visual/action minima when local ElevenLabs audio is shorter, while still expanding resolved beats to measured audio duration plus configured headroom when speech is longer.
- Remove the obsolete 15-second narration filler from the Example 11 result/scope beat; the scope now advances for six real firmware seconds and remains visible for any narration tail instead of double-counting speaker time.

- Fixed MinGW/UCRT64 compilation of the Windows Storybook system-font renderer: Win32 `SIZE::cx` is a `LONG`, so measured GDI widths now pass through checked `LONG` -> `int` conversion before renderer arithmetic instead of mixing Win32 and renderer integer types under `-Werror`.
- Guarded the Windows `WIN32_LEAN_AND_MEAN` and `NOMINMAX` definitions in the Storybook host-process launcher so MinGW/UCRT64 builds remain clean under `-Werror` when the toolchain already defines `NOMINMAX`.

### Automated tutorial / Storybook infrastructure

- Start the host-only CLOCK Storybook implementation with a versioned **schema 1** contract and explicit separation between Module Controls, Patch Actions, External Stimulus, Presentation Actions and Flow/Validation.
- Freeze Phase-1 presentation defaults at 1920x1080/30 fps, hard cuts only, firmware time frozen during pure chapter/text/callout scenes, and MP4/H.264 as the default publication target with optional WebM output.
- Define optional pre-produced intro/outro videos as publication assets rather than Storybook-rendered scenes; final subtitle sidecars will be offset by the probed intro duration.
- Add a host-only `StorySimulatorPort` that delegates accepted actions to the existing `SimulatorRuntime` while rejecting ambiguous recorded semantics: generator run/hold requires an already connected cable, RST pulses require an RST cable, controls other than POWER are rejected while powered off, and one encoder action represents exactly one detent.
- Preserve existing simulator cable semantics: connecting SYNC/RST starts the configured generator, while later generator hold/run leaves the cable state unchanged. Environment actions never mutate the production firmware `SOURCE` setting.
- Add an end-to-end Storybook boundary regression proving factory AUTO -> SYNC connect -> acquisition -> lock -> production auto-start -> real STOP -> incoming locked SYNC does not override manual STOP.
- Make POWER scriptable only through the existing simulator power-cycle boundary and reserve optional scope presentation for real simulator telemetry of the visible channel.
- Add architecture guards preventing Storybook/YAML/media dependencies from entering embedded `src/` or `lib/clock_core`, plus generated-media ignore rules so frames/videos/subtitles/reports never enter source bundles.
- Keep Storybook authoring sources under `docs/tutorials/`: versioned stories in `stories/`, shared theme data in `themes/`, and deterministic interaction timing in `interaction_profiles.yaml`; `sim/tutorial/` contains host implementation only.
- Implement the schema-1 typed Story model, strict project-local YAML subset parser, structural decoder and semantic validator without adding an external YAML dependency to firmware or simulator builds.
- Reject unknown fields, scenes, actions and CLOCK buttons; reject unsupported schema/transition semantics, undefined theme/profile resources, malformed colours, duplicate story IDs and invalid wait/assert values with story/scene/action context.
- Enforce cable/source semantics before execution: SYNC generator operations require a patched SYNC input, RST generator/pulses require a patched RST input, while external source parameters remain independent from CLOCK `SOURCE`.
- Add `storybook_parser_tests` to the Headless CMake/CTest matrix and cover literal text blocks, publication references, POWER/scope parsing, Phase-1 FADE rejection and deliberately unsupported YAML constructs.
- Add the deterministic SB-3 `StoryRunner`, central HUMAN_SLOW/NORMAL/FAST interaction-profile loading, explicit presentation-vs-firmware time handling, fixed-cadence `wait_until`, assertions and stable execution traces for later rendering.
- Add the executable `docs/tutorials/stories/external-sync.yaml` reference story and require two independent runs to produce the same logical trace while proving AUTO acquisition/lock, firmware auto-start, generator hold/reacquisition and manual STOP precedence.
- Implement `setup.factory_reset` through an erased simulator-persistence precondition and the existing power lifecycle rather than direct `ClockState` mutation.
- Add runner coverage for encoder direction/detents, encoder push, TAP, POWER, SYNC source/patch/generator, RST patch/generator/pulse, subtitles, visible-channel scope, explicit waits, timeout failures and assertion failures.
- Add the SB-4 one-way front-panel presentation layer: synchronized visible button/encoder states, deterministic SYNC/RST plug insertion/removal, scriptable scope state, real POWER/cable telemetry, and PanelLayout-only geometry.
- Move shared activity-LED visual persistence into `panel_led_visual.*` so both the native panel and Storybook derive visible LED state from the same real gate telemetry without synthesizing pulses.
- Add a deterministic headless RGBA dynamic panel layer and regressions proving custom PanelLayout coordinates, PLAY down/up synchronization, half-progress SYNC/RST insertion, visible patch plugs, real encoder telemetry, POWER OFF agreement, and actual LED-source ownership.
- Add the SB-5 full-frame software renderer for deterministic 1920x1080 RGBA8 tutorial output without desktop capture.
- Compose the real production OLED framebuffer at strict integer nearest-neighbour scale, reuse the same framebuffer inside the PanelLayout-derived module view, and keep developer instrumentation out of tutorial frames.
- Add an optional visible-channel scope backed by the existing transport-referenced scope timing model and the selected channel's real gate-transition telemetry.
- Add deterministic data-driven Storybook themes, project-local `CLOCK UI`/`CLOCK Mono` faces, explicit font sizes/line spacing/colours, and hard failure for unavailable requested fonts.
- Add measured wrapping and hard horizontal/vertical overflow rejection for chapters, explanatory text, subtitles, TIP, WARNING and RECIPE presentation.
- Add `storybook_renderer_tests` covering all Phase-1 scene kinds, theme styling/font failure, overflow rejection, exact OLED pixel ownership/scaling, subtitle isolation and real visible-channel scope traces.
- Add the SB-6 single-run frame pipeline: a read-only execution observer samples the real Story Runner at exact rational frame timestamps without replaying CLOCK.
- Generate deterministic lossless PNG/RGBA8 frames, raw-RGBA frame digests, atomic staging, and a source/revision/output manifest without adding an image-codec package dependency.
- Generate SRT and WebVTT from the same timed tutorial subtitle events used for the burned-in subtitle strip; publication-time intro offsets remain an SB-7 media-composition concern.
- Add frame-pipeline regressions for exact timestamps, byte-identical repeated PNG generation, sidecar timing, manifests, and cleanup/preservation after render failure.
- Add the SB-7 host-only publication compositor: probe optional pre-produced intro/outro assets with `ffprobe`, normalize them to the Story resolution/FPS, encode the generated tutorial as MP4/H.264, and optionally transcode the final publication to WebM/VP9.
- Preserve intro/outro audio when present by normalizing publication audio to 48 kHz stereo and inserting deterministic silence only into otherwise silent segments; keep tutorial-only publication audio-free.
- Shift final SRT/WebVTT cues by the measured normalized intro duration while leaving outro-only/tutorial-only cue timing unchanged.
- Execute `ffmpeg`/`ffprobe` through argv-safe host process calls without shell interpolation; missing tools or referenced media fail only the publication stage.
- Publish through staging plus previous-output backup/rollback so failed media work cannot replace or discard the last successful publication.
- Add integration coverage for intro+outro, intro-only, outro-only, no-clips, MP4/H.264, WebM/VP9, audio/no-audio, subtitle offsets, media normalization, and missing-asset failure preservation.
- Add the SB-8 canonical reference Storybook under `docs/tutorials/stories/`: Getting Started, PLAY/STOP, Changing Tempo, Tap Tempo, Selecting an Operating Topology, Selecting an Independent Channel, Clock Mode, Euclidean Mode, Sequencer Mode, Divider Bank, Saving and Loading a Preset, External SYNC, and External RST.
- Extend schema 1 with explicit held-button `state: down|up` and encoder-push `hold_ms` so real TAP-modifier chords and long encoder presses can be recorded without direct UI-state mutation.
- Add `storybook_reference_stories_tests`: parse, validate and execute all 13 canonical stories twice on independent simulator instances, require deterministic traces, and assert their intended production states.
- Add a 40-point Phase-1 acceptance-evidence matrix under `docs/tutorials/PHASE1_ACCEPTANCE.md`, backed by implementation/test references rather than prose-only claims.
- Run the canonical reference catalog explicitly in the native simulator CI job on Linux, Windows and macOS in addition to normal CTest coverage.
- Extend Storybook repository policy so the exact canonical story set, physical modifier/long-push authoring contracts and 40-row Phase-1 evidence matrix cannot silently regress.
- Add the user-facing `clock-storybook` headless CLI with `validate`, `frames`, and `video` commands, MP4/WebM selection, optional frame retention, explicit ffmpeg/ffprobe paths, and source-revision stamping.
- Add eleven copy-and-edit didactic example Stories under `docs/tutorials/examples/` plus `docs/tutorials/VIDEO_GENERATION.md` covering the complete build, validation, frame-only and final-video workflow without changing the canonical 13-Story CI catalog.
- Add `storybook_example_stories_tests` and a CLI validation CTest so all eleven examples are parsed, semantically validated, and executed through the real simulator/product UI without generating publication media.
- Fix Windows Storybook publication process launch by replacing CRT `_wspawnv` with direct `CreateProcessW`, preserving argv-safe execution while correctly handling spaces, quotes and trailing backslashes; add a self-spawn regression and visible CLI progress before FFmpeg publication.

### Sequencer 2.0 foundation

- Add a compact Sequencer 2.0 pattern model with **1-128 steps**, two fixed 64-bit gate words, eight pattern slots per physical channel, and pattern-local `FORWARD`, `REVERSE`, `PINGPONG`, and deterministic `RANDOM` traversal plus `LOOP`/`ONCE` end behavior.
- Keep RANDOM traversal history-free and seedable so rescheduling can reproduce the same selected step without a process-global PRNG or hidden call-order dependency.
- Add a dedicated 2-KiB persistent pattern-bank record outside the repeated `ClockState`: 64 fixed pattern slots use explicit compiler-independent records, a bank header, and CRC-32 protection.
- Extend the development logical A/B image from the released 8 KiB layout to the existing 12-KiB architectural ceiling while continuing to accept 8-KiB and older 4-KiB generations; bytes beyond an older payload read as erased and the next durable write promotes the image without changing its historical prefix.
- Seed pattern `P1` for every channel from its legacy 1.1 Sequencer settings on first Sequencer-2.0 initialization; `P2`-`P8` remain clean defaults.
- Coalesce pattern-bank writes through the existing power-loss-safe A/B storage and suppress Flash commits while transport is PLAYING.
- Add the focused `test_sequencer2` suite with 20 named tests covering 128-step bounds, traversal/ONCE behavior, deterministic RANDOM, rotation, corrupt records, legacy migration, delayed writes, interrupted commits, 8-KiB-to-12-KiB promotion and sparse step-expression persistence/defensive boundaries.
- Integrate the active P1–P8 bank into the production ClockEngine and persist the selected pattern slot per channel.
- Replace the released 16-step editor page with an **8-step viewport** over the active 1–128-step pattern; direct `EDITOR` entry, BACK navigation and long-press pattern operations follow the production hardware control model.
- Move `EDITOR` to the first Sequencer-menu row so pattern editing is the primary action; keep `PATTERN`, `LENGTH`, `ROTATE`, play `MODE`, and `LOOP` below it.
- Add sparse per-step **Probability**, Gate/relative Duty, **Tie**, and bounded **Ratchet 1–8** expression outside repeated `ClockState`; untouched steps inherit channel defaults and consume no sparse record.
- Execute step expression in the production scheduler: Probability replaces the channel probability for that step, Gate/Duty controls HIGH duration, Tie holds deterministic adjacent gates without retrigger, and Ratchet emits bounded sub-events without recursive scheduling.
- Release an active tied gate immediately when the live pattern or its step metadata changes, preventing a live edit from leaving an output HIGH until a later sequencer event.
- Redesign the 8-step editor around musical structure instead of step-number labels: quarter-beat boundaries use solid full-height grid lines, intervening step boundaries use dotted lines, and the selected edit step remains independently outlined.
- Add a live playback-head triangle while transport is running without moving the edit cursor; PLAY/PAUSE remains transport inside the editor, while encoder movement crosses viewport boundaries directly.
- Show step-expression state compactly: `%` marks Probability overrides (the selected value is shown only for the selected step), Tie keeps the connecting gate line, and Ratchet uses one row of 2–4 dots or two rows for 5–8 substeps.
- Add compact direction and LOOP/ONCE pictograms to the editor status area and bind the bottom page indicator to the editor viewport rather than the unrelated playback page.
- Keep Song Mode and USB full-device backup/restore as later work; per-step microtiming remains deliberately outside Sequencer expression because Swing/Groove already own deterministic timing displacement.
- Treat the 12-KiB source configuration as **development-only pending a real STM32F401 ELF memory-gate run**; host tests prove layout/migration behavior but do not substitute for target SRAM evidence.

### Project support

- Replace Patreon as a current sponsorship channel with **GitHub Sponsors** and **Ko-fi** in `.github/FUNDING.yml`, repository READMEs and the generated Wiki.
- Add a read-only **SUPPORT CLOCK** root-menu page with the current GitHub Sponsors and Ko-fi destinations.
- Add a canonical `docs/SUPPORT.md` source and generated Wiki page.
- Add the user-provided Ko-fi QR code to the maintained development manual without rewriting frozen 1.0/1.1 release manuals.
- Remove GitHub Sponsors: support goes through Ko-fi only. The **SUPPORT CLOCK** page shows the `KO-FI` row alone; `.github/FUNDING.yml`, `README.md`, `README_VCV.md`, `docs/SUPPORT.md`, the User Guide and the maintained manual no longer name the personal Sponsors page. Release history and frozen release manuals are unchanged.
- Use `https://ko-fi.com/southsignallab` as the only Ko-fi address: the **SUPPORT CLOCK** page (`KO-FI` row), `.github/FUNDING.yml`, `README.md`, `README_VCV.md`, `docs/SUPPORT.md` and the User Guide. The maintained manual's Ko-fi link and QR code are updated separately; frozen release manuals are unchanged.

### VCV Rack edition

- Add `vcv/` as a peer platform shell beside `sim/` for prospective builders who want to try CLOCK before building the hardware.
- Compile the real CLOCK application, engine, services, UI renderer and `clock_core` into the Rack plugin instead of maintaining a second musical implementation.
- Add a Rack-independent `ClockVcvRuntime` adapter that advances the existing 20 kHz/50-us scheduler from Rack sample time, forwards physical panel gestures, drives conditioned SYNC/RST levels and exposes the real 128x64 framebuffer plus 0/+5-V gate states.
- Embed the complete logical CLOCK persistence image in Rack patch JSON so presets/settings/Custom Grooves reuse the existing schema and migration path.
- Generate the functional 10-HP Rack panel from `sim/panel_layout.ini` instead of maintaining an independent VCV placement: OLED upper left, push encoder upper right, PLAY/TAP/STOP row, left-aligned SYNC/RST and the hardware 4x2 output/LED matrix now match the native simulator geometry.
- Generate Rack widget coordinates and control artwork from the same millimetre source and add the eight 3 mm output activity LEDs.
- Enforce an explicit one-active-instance limit for the first alpha because the current host HAL/application callback boundary is process-global; additional instances stay muted rather than corrupting shared state.
- Add `vcv_runtime_adapter_tests` to CMake/CTest and a dedicated GitHub Actions workflow that builds/packages the plugin against pinned Rack SDK 2.6.6 on Linux x64.
- Verify the generated `.vcvplugin` payload structure and Rack SDK `make install` target in CI before exposing the package as a workflow artifact.
- Add direct host-input driving for SYNC/RST, including explicit HIGH release when a virtual cable is removed, with simulator regression coverage.
- Capture SYNC/RST level transitions at Rack sample rate in a fixed allocation-free queue before replaying them into the 20 kHz CLOCK scheduler, preventing one-sample Rack trigger pulses from being lost between adjacent 50-us firmware ticks.
- Rename the Rack-visible module to **South Signal Lab Clock** and switch the generated functional panel to a black high-contrast plate with button, input and output labelling derived from the physical layout.
- Document the VCV development-preview deployment path from adapter tests to `.vcvplugin` CI artifacts and later tagged-release integration.
- Add a dedicated cross-platform VCV developer guide covering external SDK setup, adapter tests, panel generation, local build/package/install, isolated Rack profiles, package inspection, logs and manual smoke testing.
- Formalize the Rack licensing/dependency boundary: keep the SDK external, confine Rack API use to `vcv/`, reject bundled SDK/`libRack` payloads, and document the free-of-charge plugin exception assumption.
- Fix generated VCV C++ geometry literals so integral millimetre values are emitted as standard `9.0F`/`3.0F` literals instead of invalid GNU `9F`/`3F` tokens, and add a regression that rejects integer-plus-`F` output.
- Draw Rack panel lettering through a NanoVG overlay using Rack's bundled UI font because Rack's NanoSVG loader does not render SVG `<text>` nodes; keep the SVG text only for standalone previews/documentation.
- Refine the functional VCV panel hierarchy: reuse the canonical CLOCK vector wordmark at larger scale, enlarge SOUTH SIGNAL LAB, enlarge all control/port labels, render button secondary functions in grey without slash separators, keep input jack labels to IN 1 / IN 2, keep every label below its hardware, and leave the encoder intentionally unlabelled.
- Replace the bounded overlapping Rack knob/push pair with one endless relative encoder widget: vertical click-drag produces hardware-style detents, stationary click/hold remains encoder push, mouse wheel remains relative, and the indicator rotates continuously without parameter-boundary jumps.
- Make Rack encoder gesture classification exclusive: early vertical movement locks a mouse-down to rotation, stationary short clicks are replayed only on release, and stationary holds arm push after a grace period so drag and click cannot fire together.
- Reduce the VCV CLOCK wordmark scale slightly and increase jack-label clearance while preserving simulator-derived control geometry.
- Replace the manual 75 ms VCV activity-latch timer with Rack's bounded light smoothing and dark inactive panel apertures, while leaving the real 0/+5-V gate outputs instantaneous and unchanged.
- Add adapter regressions that verify factory One Clock rising-edge periodicity and eight-output phase coherence at 44.1, 48 and 96 kHz Rack sample rates.
- Add Rack-hover keyboard controls so Shift acts as held TAP/SHIFT, Enter/E as encoder push, Up/Down as encoder detents, and P/T/S-or-Backspace as the three physical buttons; this makes hardware two-control chords usable without a latch mode.
- Add a Rack context-menu **General Settings...** command that enters the existing production `GENERAL SETTINGS` page through the shared host runtime instead of duplicating settings in Rack.
- Add a repository-root `README_VCV.md` end-user guide and surface the Trial-First port in the main README with a tightly cropped in-Rack screenshot.
- Restyle the native simulator's built-in panel to the same black/white/grey functional hierarchy as the VCV panel while retaining the shared millimetre geometry and simulator instrumentation.
- Verify One Clock 25% Swing end-to-end through the Rack-independent runtime at 44.1, 48 and 96 kHz: alternating 625/375 ms gate intervals remain phase-coherent and preserve a 1 s pair period at 120 BPM.
- Harden VCV regeneration by removing obsolete encoder SVG assets instead of allowing stale checkout overlays to survive silently.
- Expand `.gitignore` for Rack SDKs, local VCV build/package output and Rack user-profile leftovers without hiding tracked VCV source/assets.
- Keep dedicated VCV adapter coverage at 100% lines, 100% functions and 96.30% decision branches.

### Test architecture and focused coverage

- Split the largest Native suites into thematic local case fragments while retaining one deterministic translation unit per PlatformIO suite; the named Native inventory is now 525 tests.
- Add focused host-coverage modes for core, realtime, sync, Sequencer 2.0, swing, settings, controls, screensavers and Easter eggs, with optional production-source narrowing for change-local diagnosis.
- Keep focused reports diagnostic only: the repository-wide 95/95/90 coverage gate remains authoritative.
- Keep the full firmware host matrix above the unchanged 95/95/90 gate after the r47e Sequencer editor/expression hardening: 95.44% lines, 98.14% functions and 90.08% decision branches.

### Documentation

- Refine the maintained post-1.1.0 user-manual layout without rewriting the frozen 1.1.0 release manual.
- Left-align all chapter headings while preserving semantic PDF navigation.
- Normalize intermediate headings to one consistent blue/bold style with no accidental outline indentation.
- Move the Contents page directly behind the cover and rebuild it as a compact full-width title/page index.
- Replace the oversized cover author block with a firmware-update QR area; author attribution remains in the Colophon.
- Reserve a compact cover line for future release-version stamping so the QR/update area survives publication.
- Constrain the Contents table to the normal publication text frame and synchronize its page numbers from the actual exported PDF before the final build.
- Normalize direct and gallery screenshot spacing and unify ordinary data-table header/body typography.
- Repair the remaining chapter-heading style drift, including the Settings Map chapter.
- Rework General Settings into a scoped settings map with dedicated master-meter, gate-length, input-role, clock-source, loss-policy and reset explanations.
- Replace the AUTHOR overflow example with the functional `CORE LIC` license row and its current production-renderer screenshot.
- Turn Groove recording into a real six-step procedure and give the source repository URL/QR its own page.
- Split the GitHub-readable User Guide into 24 manual-aligned chapter files and publish the same chapter hierarchy into the generated Wiki.
- Re-stamp the maintained post-1.1 manual cover/back-cover raster so the working source visibly identifies firmware 1.1.0 instead of carrying the stale 1.0.1 back-cover label.
- Rebuild the README opening: a light and dark hero image (`docs/assets/readme/clock-firmware-hero-{light,dark}.webp`) generated by `scripts/generate_readme_hero.py` from `CLOCK_FIRMWARE_VERSION`, the approved Revision C render and Ubuntu Sans; a short project statement; download, manual and product links; only the CI, Manual publication and Release badges, pointing to `southsignallab/eurorack-clock-firmware`; and the four Revision B bench photographs.
- `scripts/generate_readme_hero.py --check` fails when an image shows a different firmware version, when its inputs changed, or when it differs visibly from a fresh render; `scripts/check_documentation.py` runs it.
- Remove the decorative badges and the Mermaid colour classes from the README; the roadmap diagram content is unchanged.
- Remove `scripts/generate_readme_header.py`, `docs/assets/clock-header.svg` and `docs/assets/clock-header.jpg`; nothing else used them.
- Bundle Ubuntu Sans and Ubuntu Sans Mono (Ubuntu Font Licence 1.0) under `third_party/fonts/ubuntu-sans/` for the hero generator; see `THIRD_PARTY_NOTICES.md`.
- Remove the retired centered footer from all current Markdown documents, including `test/README.md` and `vcv/README.md`, the generated Wiki and the running ODT manual (page footer, cover and back-cover raster, licence page). The manual page footer now reads `CLOCK User Manual` on the left and the page number on the right. `scripts/check_documentation.py` and `scripts/build_wiki.py` now reject the footer instead of requiring it, without per-file exemptions, and `scripts/build_wiki.py` no longer strips it from source documents; release history and the frozen release manuals are unchanged.
- Fix `scripts/build_user_manual.py` on Windows: the isolated LibreOffice user profile is now passed as a valid, percent-encoded file URL (`file:///C:/...`) instead of `file://` followed by the native path, which LibreOffice rejected. Regression tests cover Windows, UNC and POSIX paths.
- Restyle the maintained manual for home printing: no colour and no filled areas apart from QR codes and screenshots. Chapter headings with a thin grey number, bold black title and a hairline; black intermediate headings; tables with a bold header, a 0.5 pt rule under the header and 0.25 pt grey rules between rows; notes with a bold keyword and a rule on the left instead of a coloured box; numbered lists show the number only; grey page footer. Page size, margins and chapter structure are unchanged; the manual stays in the Ubuntu family.
- Typeset the manual cover instead of a full-page image: `EURORACK · 3U · 10 HP`, `CLOCK`, `User Manual`, the stamp line `Firmware <version> · Digital master clock`, the front panel as a line drawing, the South Signal Lab logo (`docs/manual-source/assets/south-signal-lab-logo.svg`, website geometry in black) and the firmware-update QR. The back page is the Colophon with the logo. `scripts/prepare_release_manual.py` stamps the typeset cover through its text line; manuals up to 1.1.0 keep their stamped cover images.
- Replace the outdated panel drawing in chapter 2 with the KiCad-derived line drawing with numbered callouts and align the legend with the panel engraving (`IN 1`/`IN 2` with their factory roles SYNC/RESET). `scripts/generate_front_panel_illustration.py` now also writes `front-panel-anatomy-print.svg` and `front-panel-print.svg`; the screenshot refresh replaces the embedded drawings, and `scripts/check_user_manual.py` fails if they differ from the generated files.
- Add a print variant to `scripts/generate_manual_screenshots.py`: every catalog state is also written as `<name>-print.png`, inverted, enlarged 8× nearest-neighbour and framed by the display cut-out outline from the panel data; `manual-screenshots.tsv` lists both variants. The manual embeds only print variants at 2.2 : 1, and one sentence explains the inverted display. README, wiki and user guide keep the light-on-dark screenshots. Regenerating the catalog updates `settings-input-config.png` (scroll bar position) and `settings-support.png` (Ko-fi address).
- Give every manual figure a `Figure n.m` caption and resolve repeated screenshots: chapter 10 shows the EUCLID settings, chapter 12 the One Clock TIMING settings, the arcade flow the initials entry; the BEATKNECHT playfield moves into the BEATKNECHT paragraph.
- Generate the manual QR codes with `scripts/generate_manual_qr_codes.py` (no QR package needed): black on white without logo, for `https://github.com/southsignallab/eurorack-clock-firmware/releases`, `https://github.com/southsignallab/eurorack-clock-firmware` and `https://ko-fi.com/southsignallab`. The manual's Ko-fi text uses `https://ko-fi.com/southsignallab`. `scripts/check_user_manual.py` checks that each code encodes its address and that the frozen manuals' repository code still encodes the address of their release.
- Make the manual PDF check reject substitute fonts: with `--require-ubuntu-fonts`, any embedded font outside the Ubuntu family, or any face that is not embedded, fails the check. Menu-path arrows and list bullets in the manual use `›` and `•`, which the Ubuntu family contains; the Windows build had embedded Segoe UI for `→` and `●`.
- Export the manual PDF without LibreOffice's 300 dpi image reduction, so the screenshots keep their nearest-neighbour pixels.
- Replace the Ko-fi button image in `README.md`, `README_VCV.md` and `docs/SUPPORT.md` with the existing text link.
- Read the output of `pdfinfo`, `pdftotext`, `pdffonts`, `fc-match` and `ffprobe` as UTF-8 in `scripts/check_user_manual.py`, `scripts/update_manual_contents.py`, `scripts/prepare_release_manual.py` and `scripts/render_tutorials.py`. On Windows the locale code page (cp1252) aborted the manual build or garbled non-ASCII text unless `PYTHONUTF8=1` was set.
- Find LibreOffice on Windows in its default install folder (`%ProgramFiles%\LibreOffice\program\soffice.exe`) when it is not on `PATH`. The manual build, the PDF check and the contents update name every missing Poppler tool (`pdfinfo`, `pdftotext`, `pdffonts`) with an install hint; the build does so before the PDF export.
- Give each `scripts/run_host_tests.py` run mode its own build tree below `build/host_tests/` (`coverage`, `sanitizers`, `focus_<area>`), so a coverage run and a sanitizer run on the same checkout no longer delete each other's tree. A second run of the same mode stops with a message; an operating-system lock that ends with the process guards each tree.
- Document the Windows prerequisites of the local check suite and the manual build in `docs/DEVELOPER_README.md` (WSL2 for sanitizers, UCRT64 before Git's `mingw64` on `PATH`, Poppler, LibreOffice, Ubuntu fonts).
- Use `https://github.com/southsignallab/eurorack-clock-firmware` in the maintained manual's source-repository link, the release packaging and release notes, the manual check, the VCV plugin metadata, the Wiki builder default and `docs/WIKI.md`. Frozen release manuals keep the address of their release; the firmware update QR code and the documents that describe it follow with the firmware change.
- Name the inputs `IN 1` and `IN 2` with their factory roles SYNC and RESET in the introduction of manual chapter 2.
- Name the inputs the same way in manual chapter 1 and in the specifications table of chapter 24, which marks the pins PA8/PA9 as those of the DIY Blackpill build.
- Leave space between the wrapped repository and Ko-fi addresses and their QR codes in the manual: the QR paragraphs use full line height, so the images no longer rise into the text above.
- Point the update address in `scripts/check_documentation.py`, the User Guide and its chapter files and `docs/PROJECT_IDENTITY.md` to `https://southsignallab.com/clock/firmware`; the project identity page names the canonical repository and notes that CFF/CodeMeta do not carry it yet.
- Describe the display profiles of `platformio.ini` in the Developer Guide: SPI SSD1315 as the default and release profile, SPI SSD1306 and its `blackpill_f401cc_spi` alias, and the base environment with the `config.h` defaults. The guide no longer calls the I2C transport current or SPI planned.
- Add state diagrams for automatic transport start (AUTOSTART, front-panel and input roles, LOSS STOP) and for tempo ownership with `SOURCE = AUTO` to the User Guide chapter on external inputs.
- Show the **SAVE ERROR** and **UPDATE FIRMWARE** notices as new screenshots (`performance-save-error`, `performance-update-firmware`) in the User Guide.

### Changed: firmware update QR code

- `INFO → UPDATES` encodes `https://southsignallab.com/clock/firmware` as a version-3 QR code (error correction M, 29 modules at 2 px = 58 px, 3-px quiet zone in the white 64 × 64 field) instead of the version-2 code for `https://github.com/napolitano`. The rows are generated with `scripts/generate_manual_qr_codes.py`; a test compares them with the encoder output. Scanning on the module is not yet verified.

### Changed: downgrade lock text

- The downgrade lock shows **UPDATE FIRMWARE** instead of **SAVE LOCKED**; the text fits the notice banner with margin in the small font.

### Build and release tooling

- The release workflow stops before building when the firmware version is a stable version whose tag `v<version>` already exists on another commit (`python scripts/version.py --require-unreleased`). A released version is rebuilt from its tag.
- Rename the screenshot contract for untypeset manuals from `LEGACY_DIRECT_SCREENSHOTS` to `PRE_TYPESET_SCREENSHOTS`: it is the contract of the working manual before the typeset cover, not that of a frozen release manual.
- Add `.gitattributes` with `*.svg text eol=lf`: generated SVGs are compared byte for byte, and a Windows checkout with `core.autocrlf=true` converted them to CRLF and failed the manual checks.
- Remove the entry for an untracked local folder from `.gitignore`; local-only folders belong in `.git/info/exclude`. The repository-address scan now lists files through Git instead of naming local folders.
- Add a regression test with several pending saves in different preset and Custom Groove slots across a blocked, a deferred and a failed commit (#7).

## [1.1.0] - 2026-09-23

### CI and documentation hardening

- Fix the Groove Recorder signed-offset expression so strict Clang simulator builds with `-Wconversion -Werror` do not narrow the promoted negation result implicitly.
- Add symmetric extreme early/late recorder regression coverage.
- Move GitHub Actions cache usage to `actions/cache@v6` and release artifact upload to `actions/upload-artifact@v7`, keeping repository workflows on Node.js 24-capable action majors.
- Expand the user manual with an explicit Swing/Groove/Humanize comparison, complete Pre-Count guidance, corrected boot/power wording, semantic heading navigation, and safer inline Groove screenshots.

### Documentation and publication refresh

- Refresh the main README around the 1.1.0 release line, adding useful CI/manual/release/toolchain/coverage/license badges and making Grooves the primary visible 1.1 feature rather than leaving them buried in the roadmap.
- Add real production-renderer Custom Groove Editor and live TAP Recorder screenshots to the README and User Guide.
- Regenerate the complete deterministic OLED screenshot catalog from the current renderer, including the horizontal mode carousel, current Info screen, Custom Groove Editor/Recorder and the full-value read-only information popover.
- Expand the GitHub User Guide and editable ODT manual with factory Grooves, Custom Groove editing, TAP recording, Count-In, One-Shot/Endless capture, the 10-slot library, Rename/Delete, the horizontal mode carousel and long read-only information handling.
- Synchronize Architecture, Configuration, Timing, Simulator, manual-workspace and coverage documentation with schema v12, the frozen 10-slot Custom Groove layout, the Recorder timing contract and the verified 1.1.0 release baseline.
- Preserve historical/stable 1.0.1 release documents as release-history artifacts rather than rewriting them to describe 1.1.0 behavior.

### Changed

- Replaced the fixed 2×3 Channel Mode palette with a horizontally scrolling, center-anchored mode carousel. Only the selected mode is inverted; neighboring pictograms remain unframed and carry compact labels below them. The renderer now derives its neighbors from the mode catalog so future modes can extend the same UI without changing the layout.
- Refined Custom Groove marker occlusion with a one-pixel black quiet zone around each diamond so timing-grid guides cannot visually read through or touch hollow markers.


### Code quality and readability

- Refactor persistence startup so current/preset schema probing is expressed once and migration order reads explicitly newest-to-oldest instead of repeating low-level byte-read boilerplate.
- Split persistent-state semantic validation into named meter, rate, groove, input, display, external-sync, and channel rules so invalid-state reasoning is reviewable without decoding one compound condition.
- Restructure Settings activation as an explicit `SettingsPage` state-machine dispatcher with page-focused helpers and documented safe confirmation/discard behavior.
- Restructure short encoder activation as an explicit `Screen` dispatcher so supported UI actions and intentional no-op screens are visible in one place.
- Clarify external-sync estimator state with a named reference-pulse concept, named transport sentinel and period/filter comments that document glitch rejection and integer-Q8 smoothing behavior.
- Improve legacy persistence migration helper names and comments so byte order, schema ownership and injected defaults are explicit.
- Expand the development/contribution standard with concrete readability rules while retaining the existing real-time and coverage constraints.

### Release-candidate gap closure

- Freeze the 1.1.0 Custom Groove persistence scope at 10 named slots in the existing 8-KiB layout; keep the longer-term 99-slot target conditional on a later compact/expanded storage design and real STM32F401 ELF/RAM proof.
- Add direct Custom Groove `RENAME` and `DELETE` actions with explicit destructive confirmation and retry-safe persistence semantics.
- Add an exhaustive Groove qualification sweep across all 1-64 step lengths and 0-100% Amount values plus engine vectors spanning 20/120/999 BPM, divide/multiply/rational rates, 1/4/16/64-step grids and 0/50/100% Amount.
- Define `HIL-GRV-001..004` bench vectors for subtle, triplet-like and extreme signed Groove timing without misrepresenting those planned captures as host-test evidence.
- Correct the Info product value from the compact internal identity to `Clock`.
- Add generic read-only information overflow handling: long informational values are ellipsized with `...` in list rows and open a full-value popover on activation; editable setting values are explicitly excluded from this behavior.

### Groove Engine Stage 2 - Tap recorder

- Add `RECORD >` as a second Custom Groove input mode that writes the same signed 1-64-step microtiming pattern used by the graphical editor. Recorded grooves therefore use the existing save/load/rename/delete persistence path and can be refined later in `EDITOR >`.
- Add One-Shot and Endless capture policies. One-Shot stops after one pattern pass; Endless wraps and overwrites only steps that receive a new TAP event while leaving untouched steps intact.
- Add an independent Recorder Count-In (`OFF` or 1-64 beats, default 4), visible playhead, READY/PRECOUNT/RECORDING status, bounded FIT/32/16/8/4 zoom and the existing PLAY+TURN quick-zoom chord.
- Make PLAY start/stop recording and rewind the recorder playhead to the beginning on stop; TAP is an exclusive recording event on this screen and never changes Tap Tempo.
- Preserve high-resolution physical TAP timing through button debounce and convert taps against the live Q32 engine position rather than a separate UI clock. This keeps recording aligned to internal or externally locked musical time without adding a second scheduler.
- Share the temporary Custom Groove draft between RECORD and EDITOR, including live preview, discard semantics and persistent naming/overwrite flows.

### Groove Engine Stage 2 - Custom editor

- Make Custom Groove save completion explicit: every new slot starts with an editable generated two-word musical name, and encoder long-press commits the name/pattern immediately (`HOLD TO SAVE`). The per-boot pseudo-random seed is mixed with generation order and human interaction timing; it is convenience naming, not cryptographic randomness.
- Add `LOAD >` directly to the normal `GROOVE` settings page so a stored Custom Groove can be recalled without first entering the graphical editor.
- Resolve active Custom Groove names from the persistent slot on the Performance screen so `G:<name>` shows the actual user-visible name instead of the generic `CUSTOM` label.
- Render Custom Groove diamond markers as opaque overlays: clear the diamond footprint before drawing its outline/fill so beat and step guides cannot bleed through the marker; keep the grid continuous immediately above and below it.
- Add framebuffer regression coverage that verifies grid continuity outside an unselected marker and explicit occlusion inside it.
- Add the standard `GROOVE EDITOR` screen header and separator used by the other dedicated editors; move beat labels and the editable grid below the header while preserving the bottom status row.
- Add framebuffer regressions for the editor header, separator, reserved header area and grid/status layout.
- Add a graphical 128x64 Custom Groove editor with solid beat guides, dotted step guides, diamond timing markers, selected-marker fill, beat labels and bounded 4-step zoom.
- Keep editing non-destructive: live audition is a runtime preview only; BACK can discard the draft and restores the pre-editor Groove assignment without writing Flash.
- Support fine encoder timing moves, TAP+encoder coarse moves, TAP marker advance, START+encoder quick zoom, and the standard long-press menu for SAVE, LOAD, ZOOM and LENGTH.
- Add signed per-step microtiming for 1-64-step Custom Grooves while preserving monotonic bounded event scheduling when combined with Swing.
- Add CRC-protected fixed Custom Groove records outside repeated ClockState persistence. The current 8-KiB layout stores 10 named 64-step slots at bytes 3136-4095, preserving the legacy score records at 3072-3135 and the Top-100 region from 4096 onward.
- Advance persistence to schema v12 by appending one Custom Groove slot reference for One Clock and each Independent channel; schema v11 and all previously supported formats remain explicit migration inputs.
- Keep the accepted 99-slot UX target as future persistence work; it is not claimed by this implementation because the existing 8-KiB image cannot hold 99 full 64-step records safely.


### Configurable inputs and hardware settings

- Treat the two Rev-1 LM393 comparator paths as globally configurable `INPUT 1` / `INPUT 2` roles while preserving the physical SYNC/PA8 and RST/PA9 net identities and factory `SYNC / RESET` assignment.
- Add selectable `OFF`, `SYNC`, `RESET`, `RUN`, `START`, `STOP`, `RESTART`, and `TAP` roles; reserve `FILL` for the later Fill feature without exposing it in current selection/persistence.
- Enforce active-role uniqueness across both inputs in the Settings editor and persistence validation; allow `OFF` on both inputs.
- Keep SYNC on the existing PPQN/edge/filter/smoothing/loss path and RESET on TRIGGER/GATE semantics; make RUN level-authoritative and START/STOP/RESTART/TAP positive-edge commands.
- Preserve exact capture timestamps for external TAP and route them through the existing Tap Tempo estimator.
- Discard queued electrical edges on role reassignment so a pulse captured under one role can never be reinterpreted under another.
- Add `GENERAL SETTINGS → INPUTS` with direct INPUT 1/2 role editing and `CONFIG >` for shared external timing/reset parameters.
- Add `GENERAL SETTINGS → HARDWARE` and move ENCODER DIR / ORIENTATION into it.
- Replace the left Settings cursor with full-row inversion, reclaiming horizontal display width; active value editing is counter-inverted inside the selected row.
- Advance 1.1.0 persistence to schema v11; migrate schema-v10 records by injecting the factory `SYNC / RESET` roles. The repeated CURRENT + eight-preset footprint is now 2,801 bytes with 30 bytes of remaining in-place payload growth.
- Regenerate deterministic manual screenshots for GENERAL, INPUTS, input CONFIG and HARDWARE from the production framebuffer.

### Validation

- Native inventory: **496 named tests**; Input/SYNC suite **116 / 5,177**, Swing/Custom timing/Recorder **39 / 211,224**, Settings **88 / 290**, complete host firmware **39** cases across all tested display variants.
- Complete firmware host variants: legacy I2C **39 / 4,294**, SPI default/SSD1306/SSD1315 **39 / 3,257** each, and fixed-I2C/reset **39 / 4,277**, all with zero failures.
- Repository coverage: **9260/9667 executable lines (95.79%) / 805/819 functions (98.29%) / 5639/6246 source decision branches (90.28%)**; the 95/95/90 gates remain unchanged.
- Full ASan/UBSan Native/firmware matrix passes on the 1.1.0 release-preparation tree, including all focused suites plus legacy I2C, SPI SSD1306, SPI SSD1315 and fixed-I2C/reset-pin firmware variants.
- Headless simulator integration remains **2/2 PASS**, including the engine-phase-locked scope reference regression.

### Changed

- Reworked Channel/One Clock settings from the interim flat section list into one deliberate hierarchy level per musical group. Clock/One Clock prioritize `TIMING >`, Euclid and Sequencer prioritize their mode page, and `OUTPUT >` remains last; the Channel root therefore stays short while preserving the same importance ordering.
- Keep focused second-level tools where they carry real complexity: `GROOVE >` lives inside `TIMING >`, and Sequencer `PATTERN >` lives inside `SEQUENCER >`. BACK restores the originating group/cursor instead of returning to an unrelated default row.

### Fixed

- Phase-lock the simulator developer-scope musical ruler to the real current engine position instead of reconstructing historical references from transport start plus the current BPM/rate. Tempo, external-SYNC, and rate changes can no longer make the ruler drift relative to already captured gate transitions.
- Prevent selecting `RUN` in Settings from starting or stopping transport merely because the assigned comparator is already HIGH/LOW. RUN now captures a neutral baseline on boot and role reassignment; only a later physical level change may command transport, preserving the STOP-on-boot invariant.
- Repaired release-tooling regressions after the Groove/UI test expansion: the test-inventory meta-test now validates the inventory gate contract instead of duplicating stale exact suite counts, and the V1 forward-compatibility policy test now checks the schema-v12 2,882-byte repeated-state footprint and 21-byte in-place growth ceiling.
- Updated development and coverage documentation to the current 496-case Native inventory.

### Groove Engine Stage 1

- Add the first preset-based deterministic Groove Engine for 1.1.0. One Clock owns one shared groove; Independent mode owns groove settings per channel; Divider Bank remains groove-free.
- Add read-only factory presets `SWING 54`, `SWING 58`, `SWING 62`, `SWING 66`, `POCKET A`, `POCKET B`, and `POCKET C`, plus explicit `OFF`.
- Add `AMOUNT` (0-100%) and `ROTATE` controls without merging deterministic Groove timing with the separate Humanize layer.
- Bound combined Swing + Groove displacement so event timestamps remain monotonic and gate lengths cannot consume the next rising edge.
- Advance persistence to schema v10 while preserving the complete v9 payload as a migration prefix; migrated v9 and older states inject Groove `OFF`, amount 100%, rotation 0.
- Add Groove timing, ownership, menu, clamp, persistence and migration regressions.
- Show the active groove as `G:<preset>` on the Performance screen: lower-left for Clock/One Clock and directly above the Euclid/Sequencer pattern strip; hide the label for `OFF` or 0% Amount.
- Group Channel settings into explicit `TIMING >`, mode-specific (`CLOCK >` / `EUCLID >` / `SEQUENCER >`) and `OUTPUT >` pages, ordered by musical relevance instead of exposing one long flat parameter list.
- Keep rate, polyrhythm, Swing and Groove together under `TIMING >`; keep meter under `CLOCK >`, Euclid pattern controls under `EUCLID >`, and Sequencer length/rotation plus `PATTERN >` under `SEQUENCER >`.
- Add navigation and return-cursor regressions for every group entry/exit path, including `GROOVE >` returning to Timing and `PATTERN >` returning to Sequencer.
- Expand the Settings/complete-firmware regressions around the hierarchical menu paths and restore the hard repository-wide decision-branch coverage gate without reducing its 90% threshold.

### Pre-Count display

- Replace the shrinking-circle animation with a static black square popover framed by a one-pixel white border.
- Show the remaining count as the only large numeric element; remove phase-driven size animation and the associated continuous redraw churn.
- Add a meter-aware step row along the bottom of the popover: exactly the active beat is filled while all other beats remain outlined, with the marker wrapping to the first position on each master-meter bar.
- Keep beat-boundary redraws and the final completion redraw deterministic so the popover updates only when visible state changes.
- Update framebuffer/controller regressions and deterministic manual screenshots for the new presentation.
- Make external Pre-Count advance in configured master-meter beats instead of hard-coded quarter notes; derive each accepted SYNC pulse from PPQN and `METER UNIT`, including sparse pulses that span multiple meter beats.
- Keep scheduler interpolation from double-advancing Pre-Count while a selected external clock is locked, and add realtime regressions across half-, quarter-, eighth- and sixteenth-note master units plus 24 PPQN.

### Manual archive hygiene

- Keep `docs/manual/` limited to final stable-release ODTs; remove historical Alpha/Beta manual snapshots from the repository archive.
- Treat Alpha, Beta and RC manuals as transient publication artifacts generated from `docs/manual-source/clock-user-manual.odt`.
- Require a committed frozen ODT only for final `X.Y.Z` releases, while keeping prerelease publishing and Wiki generation functional without archive growth.
- Add documentation/tooling regressions that reject prerelease files in the stable manual archive.

### Build and manual tooling

- Separate the editable manual workspace from the published manual archive: `docs/manual-source/` now contains the working ODT and publication assets, while `docs/manual/` contains only versioned ODT manuals with matching curated release records.
- Remove obsolete intermediate Alpha/Beta manual snapshots and add a tooling regression that rejects unversioned files, subdirectories, or undocumented release manuals in `docs/manual/`.
- Restore the hard 90% source decision-branch coverage gate after the Pre-Count rendering fix by extending regression coverage for zero-count overlay defense and the Settings/non-Performance rendering path; do not lower the threshold.
- Standardize CI and release headless-simulator jobs on the `simulator-headless` CMake preset so the screenshot generator and prior simulator steps always use the same Ninja build tree.
- Make manual screenshot generation recover automatically from an existing `build/simulator-headless` cache created with a different CMake generator, preventing `Ninja` versus `Unix Makefiles` cache collisions.
- Add regression coverage for both workflow consistency and stale-generator recovery.

## [1.0.1] - 2026-09-18

### External SYNC display

- Keep the last measured external BPM on the Performance screen after lock loss when `LOSS = FREE`, matching the tempo that the engine actually continues to run.
- Keep `LOSS = INTERNAL` and non-freewheel unlocked operation on the configured internal/fallback BPM.
- Extend the framebuffer regression so locked external tempo, unlocked FREEWHEEL tempo retention, INTERNAL fallback, and INTERNAL source ownership are checked separately.

### Documentation and release tooling

- Regenerate the complete deterministic OLED screenshot catalog from the exact tagged firmware source on every release build.
- Refresh every generated OLED image embedded in the release ODT before PDF conversion, including the fixed manual figures and the Screensaver/Easter-egg galleries.
- Reject degenerate screenshot catalogs when distinct operating states accidentally render identical frames.
- Correct the manual screenshot generator so Independent-mode examples explicitly render the Independent topology instead of inheriting the One Clock factory default.
- Keep the SYNC settings screenshot focused on the timing controls so `FILTER`, `SMOOTHING`, and `TIMEOUT` are visible in the publication manual.

## [1.0.0] - 2026-09-18

CLOCK 1.0.0 is the first stable firmware release. It freezes the V1 feature set and closes the remaining external-SYNC correctness gaps found during final hardware bring-up.

### External SYNC and transport

- Require two valid selected SYNC edges before advertising external lock: the first edge starts acquisition; the second establishes a measurable period and external BPM.
- Start transport on a newly acquired valid external lock in `AUTO`/`EXTERNAL` when external auto-transport is armed.
- Preserve explicit user STOP/PAUSE precedence so incoming SYNC cannot silently restart a manually stopped or paused clock.
- Apply `LOSS = STOP` to transport as well as lock state and allow a later valid reacquisition to restart only when the stop was caused by sync loss rather than by the user.
- Fix `AUTO + FREE` so loss of lock continues at the last measured external tempo instead of falling back to the internal BPM.
- Show the measured external BPM on the Performance screen while `AUTO` or `EXTERNAL` is locked; unlocked operation continues to show the internal/fallback BPM.
- Keep RST phase-only and transport-independent in both TRIGGER and GATE modes.

### External SYNC responsiveness

- Add `SMOOTHING = OFF / LOW / MEDIUM / FULL` to the SYNC settings so tempo-following can be matched to precise digital masters, deliberately modulated clocks, or noisier analog sources.
- Make `LOW` the factory default: each new valid period contributes 75% and the previous filtered period 25%, replacing the earlier fixed 25%-new/75%-old response that could take several seconds to settle after a large tempo move.
- Define `OFF` as 100% newest period, `MEDIUM` as 50/50, and `FULL` as the legacy 25%-new/75%-old behavior. Keep the existing microsecond `FILTER` setting independent as the glitch-rejection control.
- Persist the new setting in schema v8 and migrate schema v7 records losslessly with `LOW` injected as the safe release default.

### Hardware and documentation

- Correct current hardware documentation and source comments from the obsolete 74HCT244 reference to the final 74HCT541-class output buffer.
- Synchronize the stable 1.0.0 identity across primary user/developer documentation and release metadata.
- Harden manual release stamping so firmware-version updates are limited to firmware-bearing body/metadata fields and can never rewrite independent license versions such as PolyForm Noncommercial 1.0.0.
- Document two-edge lock acquisition, external auto-start, manual transport precedence, all three sync-loss policies, and measured external BPM display behavior.

### Tests

- Add dedicated regressions for acquisition-only first pulse, second-pulse lock/auto-start, manual STOP precedence, STOP-loss/reacquisition restart, and AUTO FREEWHEEL tempo retention.
- Add a framebuffer-level Performance regression proving that a locked external source renders its measured BPM rather than the internal fallback value.
- Add simulator regressions that keep a newly opened developer scope armed when transport is already stopped after an earlier run, while attaching correctly when opened during a live PLAY epoch.
- Expand the public Native inventory to **416 named tests**, including **96** external-SYNC behavior tests, **59** settings-contract tests, and **29** complete host-firmware tests.
- Revalidate the complete host sanitizer matrix and both headless simulator tests; repository coverage remains above every hard release floor at **96.51% executable lines / 98.37% functions / 90.08% source decision branches**.
