/**
 * @file clock_types.h
 * @brief Domain types and persistent configuration model for the clock firmware.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace clockfw {

/** Number of independent gate channels provided by the module. */
inline constexpr std::size_t kChannelCount = 8U;

/** @brief Selects the source used to drive the master timeline. */
enum class ClockSource : std::uint8_t { Internal, External, Auto };

/** @brief Selects the top-level routing model used by the eight physical outputs. */
enum class OperatingMode : std::uint8_t { Independent, UnifiedClock, DividerBank };

/** @brief Selects the fixed divisor family used by the divider-bank operating mode. */
enum class DividerBank : std::uint8_t { PowersOfTwo, Integers, Primes };

/** @brief Selects the rhythm generator used by one output channel. */
enum class ChannelMode : std::uint8_t { Clock = 0U, Euclid = 1U, Sequencer = 2U, Off = 3U };

/**
 * @brief Selects the STOP-mode OLED idle animation. Values intentionally match the UI MODE
 *        numbers.
 */
enum class ScreensaverMode : std::uint8_t {
    Fractal = 1U,
    Orbit = 2U,
    None = 3U,
    Plug = 4U,
    Clock = 5U,
    Heartbeat = 6U,
    Acid = 7U,
    Spectrum = 8U,
    Field = 9U,
    Blox = 10U,
    Matrix = 11U,
    CubeCover = 12U,
    MakeMusic = 13U,
    Labyrinth = 14U,
    Starfield = 15U,
    Fireworks = 16U
};

/** @brief Represents the master transport state. */
enum class TransportState : std::uint8_t { Stopped, Paused, Playing };

/** @brief Selects whether the integer rate factor multiplies or divides the base rate. */
enum class ClockRatioMode : std::uint8_t { Multiply, Divide };

/** @brief Selects whether a channel follows a global reset or keeps its local phase. */
enum class ResetMode : std::uint8_t { Global, Free };

/** @brief Selects the external-sync edge used for timing capture. */
enum class SyncEdge : std::uint8_t { Rising, Falling };

/** @brief Selects external reset interpretation after the comparator. */
enum class ExternalResetMode : std::uint8_t { Trigger, Gate };

/**
 * @brief Assignable semantic roles for the two conditioned comparator inputs.
 *
 * Fill is reserved for the later Euclid/Sequencer fill feature and is not
 * selectable in the current 1.1 development UI.
 */
enum class InputFunction : std::uint8_t { Off, Sync, Reset, Run, Start, Stop, Restart, Tap, Fill };

/** @brief Global role assignment for the two electrically equivalent inputs. */
struct ExternalInputAssignments {
    /** Role of comparator input 1. */
    InputFunction input1 = InputFunction::Sync;
    /** Role of comparator input 2. */
    InputFunction input2 = InputFunction::Reset;
};

/** @brief Selects transport behavior after the external clock is lost. */
enum class SyncLossMode : std::uint8_t { Stop, Freewheel, Internal };

/** @brief Selects how strongly successive external-clock periods are smoothed. */
enum class SyncSmoothing : std::uint8_t { Off, Low, Medium, Full };

/** @brief Selects one read-only Stage-1 factory groove; OFF disables groove timing. */
enum class GroovePreset : std::uint8_t {
    Off = 0U,
    Swing54,
    Swing58,
    Swing62,
    Swing66,
    PocketA,
    PocketB,
    PocketC,
    Custom
};

/** @brief Deterministic factory-groove assignment. */
struct GrooveSettings {
    /** Factory groove, or Custom for a library slot. */
    GroovePreset preset = GroovePreset::Off;
    /** Groove Amount in percent, 0..100; the timing helpers clamp larger values. */
    std::uint8_t amountPercent = 100U;
    /** Pattern rotation in events, applied modulo the pattern length. */
    std::uint8_t rotation = 0U;
    /** Zero-based Custom Groove library slot used only when preset == Custom. */
    std::uint8_t customSlot = 0U;
};

/** @brief Musical meter expressed as numerator and note-value denominator. */
struct MeterSettings {
    /** Beats per bar. */
    std::uint8_t beats = 4U;
    /** Note value of one beat (4 = quarter note); the timing code treats 0 as 4. */
    std::uint8_t unit = 4U;
};

/** @brief Integer and rational rate configuration for one channel. */
struct RateSettings {
    /** Whether factor multiplies or divides the rate. */
    ClockRatioMode mode = ClockRatioMode::Multiply;
    /** Integer rate factor; 0 is treated as 1. */
    std::uint8_t factor = 1U;
    /** Numerator of the rational ratio applied with factor; 0 is treated as 1. */
    std::uint8_t numerator = 1U;
    /** Denominator of the rational ratio; 0 is treated as 1. */
    std::uint8_t denominator = 1U;
};

/** @brief Settings shared by CLOCK, EUCLID, and SEQ channel modes. */
struct CommonChannelSettings {
    /** Rhythm generator of the channel. */
    ChannelMode mode = ChannelMode::Clock;
    /** Rate relative to the master beat (CLOCK) or the sixteenth-note grid (EUCLID/SEQ). */
    RateSettings rate{};
    /** Delay of every odd event in percent of the interval; the scheduler caps it at 50. */
    std::uint8_t swingPercent = 0U;
    /** Groove applied on top of swing. */
    GrooveSettings groove{};
    /** Chance of a hit to emit a gate, percent; 0 never, 100 and above always. */
    std::uint8_t probabilityPercent = 100U;
    /** Requested gate length in milliseconds; the scheduler limits it to the interval. */
    std::uint16_t gateLengthMs = 10U;
    /** Phase offset in percent of the nominal interval; clamped to 99. */
    std::uint8_t phasePercent = 0U;
    /** Whether a global reset restarts the channel. */
    ResetMode resetMode = ResetMode::Global;
    /** True suppresses every hit of the channel. */
    bool muted = false;
};

/** @brief CLOCK-specific channel settings. */
struct ClockChannelSettings {
    /** Local meter: the unit scales the rate against the master unit, beats is the cycle. */
    MeterSettings meter{4U, 4U};
};

/** @brief Euclidean-pattern settings. */
struct EuclidSettings {
    /** Pattern length in steps; 0 produces no hits. */
    std::uint8_t steps = 16U;
    /** Hits spread over the steps; capped at steps, 0 produces no hits. */
    std::uint8_t hits = 4U;
    /** Pattern rotation in steps. */
    std::uint8_t rotation = 0U;
};

/** @brief Binary gate-sequencer settings. */
struct SequencerSettings {
    /** Pattern length in steps; 0 produces no hits. */
    std::uint8_t length = 16U;
    /** Pattern rotation in steps. */
    std::uint8_t rotation = 0U;
    /** Gate bits, bit n = step n. */
    std::uint64_t pattern = 0x1111ULL;
};

/** @brief Settings for the global one-clock-to-eight-outputs operating mode. */
struct UnifiedClockSettings {
    /** Rate of all eight outputs. */
    RateSettings rate{};
    /** Swing of all outputs in percent; the scheduler caps it at 50. */
    std::uint8_t swingPercent = 0U;
    /** Groove of all outputs. */
    GrooveSettings groove{};
    /** Gate length of all outputs in milliseconds. */
    std::uint16_t gateLengthMs = 10U;
    /** Phase offset of all outputs in percent of the interval; clamped to 99. */
    std::uint8_t phasePercent = 0U;
    /** Maximum per-output timing displacement applied only in ONE CLOCK mode. */
    std::uint16_t humanizeUs = 0U;
};

/** @brief Settings for the fixed eight-output clock-divider operating mode. */
struct DividerBankSettings {
    /** Divisor family of the eight outputs. */
    DividerBank bank = DividerBank::PowersOfTwo;
    /** Gate length of all outputs in milliseconds. */
    std::uint16_t gateLengthMs = 10U;
};

/** @brief Complete configuration for one physical output channel. */
struct ChannelConfig {
    /** Settings of every mode. */
    CommonChannelSettings common{};
    /** CLOCK-mode settings. */
    ClockChannelSettings clock{};
    /** EUCLID-mode settings. */
    EuclidSettings euclid{};
    /** Binary SEQ-mode settings, used while no Sequencer 2.0 pattern is installed. */
    SequencerSettings sequencer{};
};

/** @brief External synchronization behavior and filtering configuration. */
struct ExternalSyncSettings {
    /** Input pulses per quarter note; 0 is treated as 1. */
    std::uint8_t pulsesPerQuarterNote = 1U;
    /** Edge of the sync input that counts as a pulse. */
    SyncEdge edge = SyncEdge::Rising;
    /** Behavior after the external clock is lost. */
    SyncLossMode lossMode = SyncLossMode::Freewheel;
    /** Reset input as trigger or as level-sensitive gate. */
    ExternalResetMode resetMode = ExternalResetMode::Trigger;
    /** Smoothing of the measured pulse periods. */
    SyncSmoothing smoothing = SyncSmoothing::Low;
    /**
     * Shortest accepted pulse period in microseconds; never below the period at the
     * supported maximum tempo.
     */
    std::uint16_t glitchFilterUs = 1000U;
    /** Lock timeout in milliseconds; the effective timeout is at least two pulse periods. */
    std::uint16_t timeoutMs = 1500U;
    /**
     * SYNC AUTOSTART (decision D-01): a lock acquisition may start transport, also the first
     * one after power-up. Factory OFF.
     */
    bool autostartOnLock = false;
};

/** @brief STOP-mode display protection and idle-animation preferences. */
struct DisplayPreferences {
    /** Idle animation shown in STOP mode. */
    ScreensaverMode screensaverMode = ScreensaverMode::Clock;
    /** STOP-mode idle time before the screensaver starts, minutes. */
    std::uint8_t screensaverAfterMinutes = 2U;
    /** STOP-mode idle time before the display dims, minutes. */
    std::uint8_t dimAfterMinutes = 5U;
    /** STOP-mode idle time before the display switches off, minutes. */
    std::uint8_t offAfterMinutes = 10U;
};

/**
 * @brief Device-local front-panel preferences that must not change when a musical preset is
 *        loaded.
 */
struct DevicePreferences {
    /** True reverses the encoder direction. */
    bool encoderDirectionReversed = false;
    /** True rotates the display image by 180 degrees. */
    bool displayRotated180 = false;
};

/** @brief User-adjustable limits applied to manual and tap-tempo changes only. */
struct TempoRangeSettings {
    /**
     * Lowest manual and tap tempo in BPM. The external sync also derives its lock timeout
     * from it until a pulse period has been measured.
     */
    std::uint16_t minimumBpm = 20U;
    /** Highest manual and tap tempo in BPM. */
    std::uint16_t maximumBpm = 999U;
};

/** @brief Complete mutable application state edited by the user interface. */
struct ClockState {
    /** Routing model of the eight outputs. */
    OperatingMode operatingMode = OperatingMode::UnifiedClock;
    /** Internal/manual tempo in BPM. */
    std::uint16_t bpm = 120U;
    /** Limits of manual and tap tempo changes. */
    TempoRangeSettings tempoRange{};
    /** Master meter; its unit is one beat of the master timeline. */
    MeterSettings masterMeter{4U, 4U};
    /** Number of silent master beats counted before a STOP->PLAY start emits gates. */
    std::uint8_t preCountSteps = 0U;
    /** Foreground transport state; the engine's isPlaying() is the real-time authority. */
    TransportState transport = TransportState::Stopped;
    /** Source of the master tempo. */
    ClockSource source = ClockSource::Auto;
    /** External synchronization settings. */
    ExternalSyncSettings externalSync{};
    /** Roles of the two comparator inputs. */
    ExternalInputAssignments inputs{};
    /** Settings of all outputs in ONE CLOCK mode. */
    UnifiedClockSettings unifiedClock{};
    /** Settings of all outputs in divider-bank mode. */
    DividerBankSettings dividerBank{};
    /** STOP-mode display preferences. */
    DisplayPreferences display{};
    /** Device-local front-panel preferences. */
    DevicePreferences device{};
    /** Per-output settings used in independent mode. */
    ChannelConfig channels[kChannelCount]{};
};

}  // namespace clockfw
