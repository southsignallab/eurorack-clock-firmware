<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# 9 Clock mode

A Clock channel can also use the same deterministic Groove layer as Euclid and Sequencer. Factory or Custom Grooves shift event timing without changing the channel rate or master timeline.

Clock produces a regular derived trigger/clock on one output. Its channel owns:

- integer divide/multiply from `÷32` through `×32` using the curated rate table;
- rational ratio numerator and denominator from 1 to 16;
- local meter;
- Swing from 0–50%;
- Probability from 0–100%;
- gate length `1 / 2 / 5 / 10 / 20 / 50 / 100 ms`;
- Phase from 0–99%;
- reset policy `GLOBAL / FREE`;
- Mute.

Integer and rational rates use fixed/integer timing with remainder retention. CLOCK does not repeatedly truncate fractional intervals, so ratios such as `2:3`, `3:2`, `4:5`, or `5:4` do not accumulate long-term drift simply because an event interval is not an integer number of scheduler quanta.

`GLOBAL` means a global reset re-anchors the channel. `FREE` allows its local cycle position to survive that global reset.

### Swing, Probability, Phase and gate length

These parameters are shared by Clock, Euclid and Sequencer channels in Independent mode unless stated otherwise.

**Swing** delays alternating subdivisions while preserving the total duration of the pair. A value of 0% is straight timing; values up to 50% progressively lengthen one half and shorten the other.

**Probability** controls whether an otherwise eligible event produces a gate. It changes event emission, not the master timeline, so skipped events do not make the channel's underlying clock drift.

**Phase** offsets a channel relative to the common timeline without creating a separate free-running transport.

**Gate length** selects a fixed HIGH duration of **1, 2, 5, 10, 20, 50, or 100 ms**. It is an absolute time value, not a duty-cycle percentage. At slow rates the selected duration can be emitted directly; on a fast multiplied channel the engine shortens an otherwise-too-long pulse before the next scheduled rising edge so one gate can never consume the following event. Gate scheduling uses the 20 kHz timing service, so edge requests are resolved on the 50 µs scheduler quantum.

![Timing reference diagram comparing the ideal grid with Swing and Phase offsets and showing One Clock Humanize as small per-output displacement around the common reference.](../manual-source/assets/timing-swing-phase.svg)

How Swing, Groove and One Clock Humanize differ is compared in [12 One Clock](12-one-clock.md#shared-timing-layers).
