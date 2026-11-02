<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# CLOCK VCV Rack Trial-First Port

Status: **EXPERIMENTAL / post-1.1.0**.

The VCV Rack track exists so prospective builders can try CLOCK before committing to the hardware build. Its product requirement is therefore unusually strict: it should behave like CLOCK, not merely reproduce a similar feature list.

The initial implementation lives in [`../vcv/`](../vcv/). It builds the production CLOCK sources and uses the same host runtime boundary already exercised by the native simulator. The Rack-specific code is responsible for Rack sample-time adaptation, Rack ports/controls, framebuffer presentation and Rack patch serialization. Musical timing, Groove/Euclid/Sequencer behavior, transport, settings and rendering stay in the production code. Front-panel geometry is not Rack-specific: `sim/panel_layout.ini` is the shared millimetre source for the simulator, manual illustration and generated Rack panel.

## Functional panel contract

The Trial-First module must teach the hardware layout rather than merely expose the same feature list. `scripts/generate_vcv_panel.py` therefore builds the Rack panel from the KiCad-derived artwork `docs/assets/front-panel/clock-front-panel.svg` and the C++ widget coordinates from `sim/panel_layout.ini`; `scripts/import_kicad_front_panel.py` writes both from the KiCad front-panel project. The geometry is the hardware 10 HP / 3U arrangement: OLED window at the upper left, DATA/ENTER encoder at the upper right, the three buttons across one row, the two inputs below, and eight output jacks in a 4x2 matrix with 3 mm activity LEDs above and to the right of their outputs.

The VCV plate shows the real engraving of the hardware panel: **CLOCK / DIGITAL MASTER CLOCK**, **DATA / ENTER**, **START / PAUSE**, **TAP TEMPO / SHIFT**, **STOP / BACK**, **IN 1**, **IN 2**, the boxed output numbers **1–8** and the South Signal Lab mark. Rack tooltips use the same names. Panel changes belong in KiCad and are imported, then regenerated into Rack; hand-editing an independent VCV placement or lettering is a regression.

The encoder uses one Rack-specific endless interaction surface without changing physical geometry. On mouse-down the gesture starts in a neutral pending state. Early **vertical** movement locks that entire gesture to rotation and produces discrete hardware-style detents; horizontal jitter is ignored for classification. If the pointer remains stationary, a short click is emitted only on release, while a held gesture becomes encoder push after a short grace period so the production long-press path still works. Once a gesture is classified as rotate or push, it cannot change type until release. Mouse-wheel events remain direct relative detents.

Rack keyboard input supplies the second hand needed for hardware chords: while the module is hovered, **Shift** holds TAP TEMPO/SHIFT, **Enter/E** holds encoder push, **Up/Down** generates encoder detents, **P** drives START/PAUSE, **T** drives TAP, and **S/Backspace** drives STOP/BACK. This makes `Shift+Enter` equivalent to TAP TEMPO/SHIFT + encoder push and `Shift+Up/Down` equivalent to TAP TEMPO/SHIFT + encoder turn without introducing a latch mode.

The module context menu also exposes **General Settings...**. That command is marshalled onto the Rack audio/runtime thread and opens the production CLOCK `GENERAL SETTINGS` page through a simulator-only host navigation hook. It does not create a Rack-specific settings model; BACK returns through the normal CLOCK UI hierarchy.

The engraving is drawn geometry in KiCad, so `CLOCK.svg` contains no `<text>` nodes and Rack's NanoSVG renders it as is; no lettering overlay or extra font is needed. NanoSVG ignores clip paths, so engraving that runs into a hole is covered by the Rack jack, LED and button widgets instead of being clipped.

Output LEDs are presentation only: Rack's normal light smoothing gives short gates visible persistence without a manually latched hold timer. OUT 1–8 themselves remain tied to the instantaneous production gate state and are not pulse-stretched.

## Current verified boundary

The Rack-independent adapter is built and exercised by normal CMake/CTest. It has been verified locally against the production source for:

- normal CLOCK boot path;
- PLAY-driven engine start;
- eight coherent 0/+5-V logical gate outputs;
- factory One Clock edge periodicity at 44.1, 48 and 96 kHz host sample rates;
- the real CLOCK OLED framebuffer;
- held encoder push through the real long-press path;
- raw CLOCK persistence-image export/import;
- Rack-sample input transition capture before the 20 kHz firmware boundary, including one-sample SYNC/RST pulses.

This does **not** yet constitute a real Rack SDK build or in-Rack runtime test. The repository CI workflow `.github/workflows/vcv.yml` performs the first real SDK build using the pinned Rack SDK.

## Deliberate first-alpha limitation

The current host HAL and application callback thunks are process-global. The first VCV alpha therefore supports exactly one active CLOCK instance per Rack process. Additional instances are disabled rather than allowed to corrupt each other's timer/GPIO/input state.

The correct multi-instance solution is an instance-context host HAL shared by `sim/` and `vcv/`, followed by removal of the static single-active-application assumptions. That refactor is a separate architecture step.

## Deployment path

Development path:

```text
production CLOCK code
        -> Rack-independent VCV adapter tests
        -> Rack SDK plugin build
        -> .vcvplugin CI artifact
        -> manual Rack smoke test
        -> Windows/macOS/Linux package matrix
        -> tagged CLOCK release artifact under dist/vcv/
```

See [`VCV_DEVELOPMENT.md`](VCV_DEVELOPMENT.md) for the complete local SDK/build/package/install/smoke-test workflow and [`../vcv/README.md`](../vcv/README.md) for the adapter overview.

<h6 align="center">From Munich with &#9829;</h6>
