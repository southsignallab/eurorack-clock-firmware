/**
 * @file clock_engine.h
 * @brief Deterministic shared scheduler for all eight gate channels.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "domain/clock_types.h"
#include "domain/custom_groove.h"
#include "domain/sequencer_pattern.h"
#include "domain/sequencer_step_metadata.h"
#include "hal/gate_output_driver.h"

namespace clockfw::engine {

/** @brief Atomic read-only snapshot consumed by the user interface. */
struct EngineSnapshot {
    /** True while the scheduler runs, including an active pre-count. */
    bool playing = false;
    /** Master timeline position in Q32 master beats (2^32 = one master meter beat). */
    std::uint64_t masterPositionQ32 = 0U;
    /** Elapsed part of the current master beat, Q32 (0..2^32-1). */
    std::uint64_t masterBeatPhaseQ32 = 0U;
    /** Completed master beats since the last restart; wraps at 2^32. */
    std::uint32_t masterBeatSerial = 0U;
    /** Current beat in the bar, 1..beats per bar. */
    std::uint8_t masterBeat = 1U;
    /** Current bar, 1..999; wraps back to 1. */
    std::uint16_t masterBar = 1U;
    /** True while the STOP->PLAY count-in runs. */
    bool preCountActive = false;
    /** Count-in beats still to go. */
    std::uint8_t preCountRemaining = 0U;
    /** Elapsed part of the current count-in beat, Q32. */
    std::uint64_t preCountPhaseQ32 = 0U;
    /** True while the external clock is locked. */
    bool externalLocked = false;
    /** True while a level-sensitive external reset holds the engine. */
    bool externalResetHeld = false;
    /** External tempo in milli-BPM; 0 when none is known. */
    std::uint32_t externalBpmMilli = 0U;
    /** Zero-based step of the last processed event per channel, for display. */
    std::uint8_t channelStep[kChannelCount]{};
};

/**
 * @brief Runs the master timeline, channel schedulers, probability gates, and gate-off timing.
 *
 * The engine is intentionally independent from OLED rendering and control-panel polling.
 * Only the periodic timer ISR calls processSchedulerTick(); configuration mutations are
 * copied into the engine inside interrupt-safe critical sections.
 */
class ClockEngine final {
  public:
    /** @brief Packed DEFAULT/Probability/Gate/Tie/Ratchet values of steps 0..127 of a channel. */
    using StepMetadataTable = std::array<SequencerStepMetadataWord, kSequencerMaximumSteps>;

    /**
     * @brief Constructs the engine around the physical gate output driver.
     * @param gateOutputs HAL driver used for all gate/LED GPIO state changes.
     */
    explicit ClockEngine(hal::GateOutputDriver& gateOutputs);

    /**
     * @brief Initializes runtime state from the complete user configuration.
     * @param state Application state to copy into the real-time engine.
     */
    void begin(const ClockState& state);

    /**
     * @brief Copies all mutable configuration into the real-time engine.
     * @param state Current application state.
     * @param rescheduleChannels True to restart all local channel schedules from now.
     */
    void updateConfiguration(const ClockState& state, bool rescheduleChannels);

    /**
     * @brief Updates only the internal master tempo with a minimal critical section.
     * @param bpm New internal/manual tempo in BPM.
     *
     * This hot-path update avoids resolving and copying all eight channels for every
     * performance-screen encoder detent. External-sync tempo remains independent.
     */
    void updateMasterTempo(std::uint16_t bpm);

    /**
     * @brief Updates one channel without disturbing the other seven channels.
     * @param channelIndex Zero-based channel index.
     * @param channelConfiguration New channel configuration.
     * @param rescheduleChannel True to rebuild the selected channel's next event time.
     */
    void updateChannel(
        std::size_t channelIndex,
        const ChannelConfig& channelConfiguration,
        bool rescheduleChannel);

    /**
     * @brief Installs one active Sequencer 2.0 pattern snapshot for a channel.
     * @param channelIndex Zero-based channel index.
     * @param pattern Valid compact 1..128-step pattern.
     * @param rescheduleChannel True to rebuild the selected channel's next event position.
     */
    void updateSequencerPattern(
        std::size_t channelIndex,
        const SequencerPatternV2& pattern,
        bool rescheduleChannel = true);

    /**
     * @brief Installs the prepared 128-step expression snapshot for one active Sequencer pattern.
     * @param channelIndex Zero-based channel index.
     * @param metadata Packed DEFAULT/Probability/Gate/Tie/Ratchet values for steps 0..127.
     */
    void updateSequencerStepMetadata(std::size_t channelIndex, const StepMetadataTable& metadata);

    /**
     * @brief Replaces one in-memory Custom Groove library slot used by real-time scheduling.
     * @param slotIndex Zero-based Custom Groove slot.
     * @param pattern Valid bounded pattern loaded from the durable Custom Groove store.
     * @param rescheduleAffected True to rebuild channels currently referencing this slot.
     */
    void updateCustomGrooveSlot(
        std::uint8_t slotIndex,
        const CustomGroovePattern& pattern,
        bool rescheduleAffected = true);

    /**
     * @brief Applies one non-persistent Custom Groove preview to a runtime channel.
     * @param channelIndex Zero-based channel index; out-of-range calls are ignored.
     * @param pattern Valid Custom Groove pattern; an invalid pattern is ignored.
     * @param amountPercent Groove Amount in percent, 0..100; larger values are ignored.
     * @param rotation Pattern rotation in steps, below the pattern length; otherwise ignored.
     * @param rescheduleChannel True to rebuild the channel's next event position.
     *
     * Foreground only: stages the pattern in the spare buffer and swaps the groove table.
     */
    void setCustomGroovePreview(
        std::size_t channelIndex,
        const CustomGroovePattern& pattern,
        std::uint8_t amountPercent = 100U,
        std::uint8_t rotation = 0U,
        bool rescheduleChannel = true);

    /**
     * @brief Clears a runtime-only Custom Groove preview from one channel.
     * @param channelIndex Zero-based channel index; out-of-range calls are ignored.
     * @param rescheduleChannel True to rebuild the channel's next event position.
     */
    void clearCustomGroovePreview(std::size_t channelIndex, bool rescheduleChannel = true);

    /** @brief Starts or resumes scheduler progression. */
    void play();

    /** @brief Pauses scheduler progression and forces all currently active gates LOW. */
    void pause();

    /** @brief Stops scheduler progression and resets the master/global channel phase. */
    void stop();

    /**
     * @brief Interrupt variant of play(): constructs no InterruptLock (F-030).
     *
     * For the scheduler interrupt, which nothing it races with can preempt, or for
     * foreground code that already holds the lock. Prepares and installs a fresh
     * start in one step, like resetGlobalPhaseFromIsr().
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock. A fresh start computes the restart schedules in place
     *      (64-bit divisions) and stages the gate changes of the restart.
     */
    void playFromIsr();

    /**
     * @brief Interrupt variant of stop(): constructs no InterruptLock (F-030), see playFromIsr().
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock. Computes the restart schedules in place (64-bit divisions).
     */
    void stopFromIsr();

    /**
     * @brief Decides under the lock whether a prepared start still applies; gets the caller's
     *        context.
     */
    using StartCondition = bool (*)(void* context);

    /**
     * @brief Starts like play() if @p condition, called under the lock, returns true (F-075).
     * @param condition Called exactly once, under the lock that installs the start; it
     *        may update caller state guarded by the same decision.
     * @param context Passed to @p condition.
     * @return The value @p condition returned; false means nothing started.
     *
     * The restart is prepared outside the lock and the lock only swaps it in, so a
     * caller that must decide and start atomically needs no lock of its own around it.
     */
    [[nodiscard]] bool playWhen(StartCondition condition, void* context);

    /** @brief Applies a global reset while respecting each channel's GLOBAL/FREE reset policy. */
    void resetGlobalPhase();

    /**
     * @brief ISR-safe global reset variant for an already-conditioned external reset edge.
     *
     * The caller must already execute in an interrupt context that cannot race the
     * scheduler ISR. This method deliberately does not manipulate interrupt state.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock. Computes the restart schedules in place (64-bit divisions).
     */
    void resetGlobalPhaseFromIsr();

    /**
     * @brief Updates the external synchronization lock information.
     * @param locked True when the external clock estimator is locked.
     * @param bpmMilli Smoothed external tempo in milli-BPM.
     */
    void setExternalLock(bool locked, std::uint32_t bpmMilli);

    /**
     * @brief ISR-safe external lock update used by the real-time input controller.
     * @param locked True when the external clock estimator is locked.
     * @param bpmMilli Smoothed external tempo in milli-BPM.
     *
     * A change of either value advances the schedule generation; losing the lock also
     * clears the external phase reference.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    void setExternalLockFromIsr(bool locked, std::uint32_t bpmMilli);

    /**
     * @brief Applies or releases a level-sensitive external reset hold from scheduler context.
     * @param high True while the reset input holds the engine; the scheduler then only ends
     *        gates and does not advance the timeline.
     *
     * Every change of the level restarts the runtime (resetRuntime()); an unchanged level
     * does nothing.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock. A change computes the restart schedules in place.
     */
    void setExternalResetGateFromIsr(bool high);

    /**
     * @brief Accepts one phase reference from an external sync source.
     * @param bpmMilli Filtered external tempo in milli-BPM.
     * @param pulsesPerQuarterNote Number of input pulses per quarter note.
     *
     * The first pulse establishes a shared musical epoch for GLOBAL channels;
     * subsequent pulses snap the master timeline to the exact rational pulse grid.
     */
    void acceptExternalPulse(std::uint32_t bpmMilli, std::uint8_t pulsesPerQuarterNote);

    /**
     * @brief ISR-safe variant of acceptExternalPulse().
     * @param bpmMilli Filtered external tempo in milli-BPM.
     * @param pulsesPerQuarterNote Number of input pulses per quarter note.
     *
     * The caller must already execute in an interrupt context that cannot race the
     * scheduler ISR. This method deliberately does not manipulate global interrupt state.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock. The first pulse after lock and the end of a count-in
     *      compute the restart schedules in place (64-bit divisions).
     */
    void acceptExternalPulseFromIsr(std::uint32_t bpmMilli, std::uint8_t pulsesPerQuarterNote);

    /**
     * @brief ISR-only: opens the quantum whose gate changes processSchedulerTick() writes once.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    void beginSchedulerQuantumFromIsr();

    /**
     * @brief Releases the gate output stage with engine and driver both LOW (F-031).
     *
     * While the stage is muted the driver holds every source LOW, whatever the engine
     * recorded. One interrupt lock lowers the engine's gate state, writes all sources LOW and
     * enables the stage, so the next event raises its gate at the jack. Foreground only.
     */
    void enableOutputStage();

    /**
     * @brief ISR-only: returns the scheduler ticks that elapsed up to one TIM3 update (F-012).
     * @param updateTimestampUs Microsecond timestamp (TIM5) of the latest scheduler update event.
     * @return Elapsed ticks since the previous call, rounded to whole ticks and bounded by
     *         config::kMaximumSchedulerCatchUpTicks; zero when this update was already counted.
     *
     * @isr TIM3 scheduler interrupt: at most config::kMaximumSchedulerCatchUpTicks loop
     *      rounds, no division, non-blocking, no allocation, opens no interrupt lock.
     */
    std::uint32_t elapsedSchedulerTicksFromIsr(std::uint32_t updateTimestampUs);

    /**
     * @brief Executes one deterministic scheduler quantum and writes its gate edges once.
     * @param elapsedTicks Scheduler ticks this quantum covers (see elapsedSchedulerTicksFromIsr()).
     *
     * This method is ISR-only and must remain bounded, allocation-free, and non-blocking.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels, at most eight events and seven
     *      ratchet sub-events per channel), non-blocking, no allocation, opens no interrupt
     *      lock. Ends with one write of all staged gate changes.
     */
    void processSchedulerTick(std::uint32_t elapsedTicks = 1U);

    /**
     * @brief Returns interrupt-safe snapshot of the timing state used by the UI.
     * @return Copy of the timing state, taken under one interrupt lock.
     */
    EngineSnapshot snapshot() const;

    /**
     * @brief Returns whether the scheduler is running, including an active pre-count.
     *
     * Read under an interrupt lock. The scheduler ISR can start the engine on its
     * own (RUN level, START/RESTART input, lock acquisition), so this is the
     * authority for real-time safety decisions, not the foreground transport field.
     *
     * @return True while the scheduler runs or counts in.
     */
    bool isPlaying() const;

    /**
     * @brief Returns the selected output's nominal event interval in Q32 master-beat units.
     * @param channelIndex Zero-based channel index.
     * @return Interval without phase, swing, groove or humanize, at least one scheduler tick;
     *         0 for an out-of-range channel.
     *
     * Foreground: copies the external tempo under a short lock, divides after it.
     */
    std::uint64_t nominalIntervalQ32(std::size_t channelIndex) const;

    /**
     * @brief Returns the position of event serial 0 of a channel: schedule epoch plus phase.
     *
     * Groove Record aligns its grid to it (F-022). The epoch is read under a short lock;
     * the phase offset is computed after it.
     *
     * @param channelIndex Zero-based channel index.
     * @return Position in Q32 master beats, saturated at UINT64_MAX; 0 for an out-of-range
     *         channel.
     */
    std::uint64_t channelGridOriginQ32(std::size_t channelIndex) const;

    /**
     * @brief Estimates the engine position at an earlier microsecond timestamp.
     *
     * This compensates front-panel debounce latency for Groove Record without
     * changing normal button debounce semantics. The estimate is quantized to
     * the 20 kHz scheduler cadence and bounded to recent timestamps.
     *
     * @param eventTimestampUs Microsecond timestamp of the event, same time base as @p nowUs.
     * @param nowUs Current microsecond timestamp; the unsigned difference is wrap-safe.
     * @return Master position in Q32 master beats; the look-back is limited to 100 ms and the
     *         result does not go below 0.
     */
    std::uint64_t estimateMasterPositionAtUs(std::uint32_t eventTimestampUs, std::uint32_t nowUs)
        const;

#ifdef CLOCK_HOST_TEST
    /** @brief Returns how many full runtime resets were executed in deterministic host tests. */
    std::uint32_t resetRuntimeCountForTest() const;
    /** @brief Returns the channels the engine currently holds HIGH (bit n = channel n). */
    std::uint8_t gateHighMaskForTest() const {
        return gateHighMask_;
    }
    /** @brief Returns how many rising edges moved by one tick because the gate was still HIGH. */
    std::uint32_t deferredGateRiseCountForTest() const;
    /** @brief Returns how often the tick catch-up reached config::kMaximumSchedulerCatchUpTicks. */
    std::uint32_t schedulerCatchUpLimitCountForTest() const;

    /** @brief Hook that runs between preparing and swapping a configuration (simulated ISR). */
    using ConfigurationSwapHookForTest = void (*)(void* context);
    /** @brief Installs @p hook with @p context; nullptr removes it. */
    void setConfigurationSwapHookForTest(ConfigurationSwapHookForTest hook, void* context) {
        configurationSwapHookForTest_ = hook;
        configurationSwapHookContextForTest_ = context;
    }
    /** @brief Returns how many schedules or step phases were computed under an InterruptLock. */
    std::uint32_t scheduleCalculationsUnderLockForTest() const {
        return scheduleCalculationsUnderLockForTest_;
    }
    /** @brief Returns how many staged swaps computed under the lock (all staged paths). */
    std::uint32_t configurationSwapFallbackCountForTest() const {
        return configurationSwapFallbackCountForTest_;
    }

    /** @brief Returns the engine's in-RAM copy of one Custom Groove library slot. */
    const CustomGroovePattern& customGrooveSlotForTest(std::uint8_t slotIndex) const {
        return *grooveSources_->slots[slotIndex < kCustomGrooveSlotCount ? slotIndex : 0U];
    }

    /** @brief Returns the engine's active Sequencer 2.0 pattern of one channel. */
    const SequencerPatternV2& sequencerPatternForTest(std::size_t channelIndex) const {
        return *sequencerPatterns_[channelIndex < kChannelCount ? channelIndex : 0U];
    }

#endif

  private:
    /** @brief Real-time copy of configuration values needed inside the ISR. */
    struct EngineConfiguration {
        /** Internal/manual tempo in BPM. */
        std::uint16_t bpm = 120U;
        /** Master meter beats per bar. */
        std::uint8_t beatsPerBar = 4U;
        /** Master meter note value (4 = quarter note); the calculations treat 0 as 4. */
        std::uint8_t beatUnit = 4U;
        /** Count-in length in master beats; 0 disables it. */
        std::uint8_t preCountSteps = 0U;
        /** Source of the master tempo. */
        ClockSource source = ClockSource::Internal;
        /** Behavior after the external clock is lost. */
        SyncLossMode syncLossMode = SyncLossMode::Freewheel;
        /** Routing model of the eight outputs. */
        OperatingMode operatingMode = OperatingMode::Independent;
        /** ONE CLOCK humanize amount in microseconds; 0 outside ONE CLOCK mode. */
        std::uint16_t unifiedHumanizeUs = 0U;
        /** Resolved configuration of each physical output. */
        ChannelConfig channels[kChannelCount]{};
    };

    /**
     * @brief Custom Groove patterns the schedule calculations read (F-071, F-072).
     *
     * The scheduler reads the table that grooveSources_ points to. A change fills a
     * spare pattern buffer and a copy of the table outside the interrupt lock; the
     * lock swaps the table pointer, so no pattern is copied under it.
     */
    struct GrooveSources {
        /** In-memory Custom Groove library; Flash is never read from scheduler context. */
        CustomGroovePattern* slots[kCustomGrooveSlotCount]{};
        /** Runtime-only preview per channel; used while previewActive is set. */
        CustomGroovePattern* previews[kChannelCount]{};
        /** Groove Amount of each preview in percent, 0..100. */
        std::uint8_t previewAmount[kChannelCount]{};
        /** Rotation of each preview in pattern steps. */
        std::uint8_t previewRotation[kChannelCount]{};
        /** True while the channel plays its preview instead of its configured groove. */
        bool previewActive[kChannelCount]{};
    };

    /** Library slots, one preview per channel and one spare buffer. */
    static constexpr std::size_t kCustomGrooveBufferCount =
        static_cast<std::size_t>(kCustomGrooveSlotCount) + kChannelCount + 1U;
    /** Storage of every Custom Groove pattern; the groove tables point into it. */
    CustomGroovePattern customGrooveBuffers_[kCustomGrooveBufferCount]{};
    /** The pattern buffer no table refers to; only the foreground writes it. */
    CustomGroovePattern* spareCustomGroove_ = nullptr;
    /** Installed and staged groove table; only the foreground writes the staged one. */
    GrooveSources grooveSourceBuffers_[2]{};
    /** Groove table the scheduler reads; swapped under the interrupt lock. */
    GrooveSources* grooveSources_ = &grooveSourceBuffers_[0];

    /** One pattern per channel and one spare; the lock swaps pointers (F-072). */
    SequencerPatternV2 sequencerPatternBuffers_[kChannelCount + 1U]{};
    /** Installed Sequencer 2.0 pattern of each channel, into sequencerPatternBuffers_. */
    SequencerPatternV2* sequencerPatterns_[kChannelCount]{};
    /** Pattern buffer no channel uses; the foreground stages the next pattern in it. */
    SequencerPatternV2* spareSequencerPattern_ = nullptr;
    /** One expression table per channel and one spare; the lock swaps pointers (F-071). */
    StepMetadataTable sequencerStepMetadataBuffers_[kChannelCount + 1U]{};
    /** Installed expression table of each channel, into sequencerStepMetadataBuffers_. */
    StepMetadataTable* sequencerStepMetadata_[kChannelCount]{};
    /** Expression table no channel uses; the foreground stages the next table in it. */
    StepMetadataTable* spareSequencerStepMetadata_ = nullptr;
    /** True once a Sequencer 2.0 pattern was installed for the channel; never cleared. */
    bool sequencerPatternV2Active_[kChannelCount]{};
    /**
     * @brief Cached smallest adjacent Custom Groove event spacing per channel (Q16 of one
     *        interval).
     *
     * Computed in the foreground whenever the channel configuration, a library slot, or a
     * preview changes, and installed inside the same critical section as that change.
     * The scheduler only reads it; it is tempo-independent. Zero (no Humanize headroom)
     * until begin() installs the first configuration.
     */
    std::uint32_t customGrooveSpacingQ16_[kChannelCount]{};

    /** @brief Mutable scheduling state for one output channel. */
    struct ChannelRuntime {
        /** Position of the next event, Q32 master beats, with phase, swing, groove, humanize. */
        std::uint64_t nextEventQ32 = 0U;
        /** Epoch the channel's event grid is anchored to, Q32. */
        std::uint64_t scheduleEpochQ32 = 0U;
        /** Serial of the next event, counted from the epoch. */
        std::uint64_t nextEventSerial = 0U;
        /** Zero-based pattern step the next event evaluates. */
        std::uint8_t step = 0U;
        /** Zero-based step of the last processed event (display). */
        std::uint8_t displayedStep = 0U;
        /** schedulerTickCounter_ value at which the gate falls (wrap-safe signed comparison). */
        std::uint32_t gateOffTick = 0U;
        /** True while gateOffTick is armed. */
        bool gateOffScheduled = false;
        /** Logical gate state the engine records. */
        bool gateHigh = false;
        /** True when lastEventSlotQ32 holds a processed event. */
        bool lastEventSlotValid = false;
        /** True while a Sequencer tie holds the gate HIGH into the next event. */
        bool tieHold = false;
        /** Nominal grid slot of the next event: epoch plus base offset, Q32. */
        std::uint64_t nextEventSlotQ32 = 0U;
        /** Grid slot of the last processed event; a reschedule never repeats it (F-063). */
        std::uint64_t lastEventSlotQ32 = 0U;
        /** Ratchet sub-events still to emit for the current step. */
        std::uint8_t ratchetRemaining = 0U;
        /** Master position of the next ratchet sub-event, Q32. */
        std::uint64_t nextRatchetQ32 = 0U;
        /** Spacing of the ratchet sub-events, Q32 master beats. */
        std::uint64_t ratchetSpacingQ32 = 0U;
        /** Gate length of each ratchet sub-event in scheduler ticks. */
        std::uint32_t ratchetGateTicks = 1U;
        /** Xorshift32 state of the probability gate; begin() seeds it per channel. */
        std::uint32_t randomState = 0x12345678U;
    };

    /**
     * @brief Inputs of the schedule calculations: the live configuration in scheduler
     *        context, or a staged one prepared outside the interrupt lock (F-068).
     */
    struct ScheduleInputs {
        /** Configuration the calculation reads. */
        const EngineConfiguration* configuration = nullptr;
        /** Humanize spacing cache that belongs to that configuration (one per channel). */
        const std::uint32_t* customGrooveSpacingQ16 = nullptr;
        /** Custom Groove library and previews that belong to that configuration. */
        const GrooveSources* grooves = nullptr;
        /** Effective master tempo in milli-BPM. */
        std::uint32_t bpmMilli = 0U;
        /** Shortest representable interval at that tempo, Q32 master beats. */
        std::uint64_t minimumIntervalQ32 = 1U;
    };

    /** @brief Scheduler state one channel schedule is computed from. */
    struct ChannelScheduleBasis {
        /** Master position the next event must not lie before, Q32 master beats. */
        std::uint64_t positionQ32 = 0U;
        /** Epoch of the channel grid (the global epoch for GLOBAL channels), Q32. */
        std::uint64_t epochQ32 = 0U;
        /** Grid slot of the last processed event, Q32. */
        std::uint64_t lastEventSlotQ32 = 0U;
        /** True when lastEventSlotQ32 holds a processed event. */
        bool lastEventSlotValid = false;
        /** True when an event exactly at positionQ32 may still be selected. */
        bool mayUseCurrentBoundary = false;
    };

    /**
     * @brief A computed channel schedule; installing it needs no division.
     *
     * Trivially constructible like PreparedStepPhase: the producers write every
     * field, so no lock or interrupt path clears it byte by byte first.
     */
    struct PreparedChannelSchedule {
        /** Value for ChannelRuntime::scheduleEpochQ32, Q32 master beats. */
        std::uint64_t epochQ32;
        /** Value for ChannelRuntime::nextEventSerial. */
        std::uint64_t nextEventSerial;
        /** Value for ChannelRuntime::nextEventQ32, Q32 master beats. */
        std::uint64_t nextEventQ32;
        /** Value for ChannelRuntime::nextEventSlotQ32, Q32 master beats. */
        std::uint64_t nextEventSlotQ32;
    };
    static_assert(
        std::is_trivially_default_constructible<PreparedChannelSchedule>::value,
        "A schedule must not be cleared byte by byte under the lock");

    /**
     * @brief A computed pattern step phase; FREE channels are bounded at install.
     *
     * No default member initializers, so PreparedRestart stays trivially constructible
     * in the scheduler interrupt; every user value-initializes it with {}.
     */
    struct PreparedStepPhase {
        /**
         * True when step and displayedStep follow from the event serial (GLOBAL or
         * Sequencer 2.0); false keeps the FREE local phase and only bounds it.
         */
        bool derivedFromSerial;
        /** Value for ChannelRuntime::step when derivedFromSerial is set. */
        std::uint8_t step;
        /** Value for ChannelRuntime::displayedStep when derivedFromSerial is set. */
        std::uint8_t displayedStep;
    };
    static_assert(
        std::is_trivially_default_constructible<PreparedStepPhase>::value,
        "A restart must not clear its prepared step phases byte by byte");

    /** @brief Configuration, groove table, pattern and Humanize spacings that one swap installs. */
    struct StagedConfiguration {
        /** The buffer the scheduler does not read (inactiveConfiguration()), or the live one. */
        EngineConfiguration* configuration = nullptr;
        /** Humanize spacing cache of the staged configuration, Q16 of one interval. */
        std::uint32_t customGrooveSpacingQ16[kChannelCount]{};
        /** Groove table to install: the inactive one with the change, or the live one. */
        GrooveSources* grooves = nullptr;
        /** Pattern buffer that the swap releases; it becomes the spare (nullptr: none). */
        CustomGroovePattern* releasedGroove = nullptr;
        /** Channel whose Sequencer 2.0 pattern the swap replaces; kChannelCount for none. */
        std::size_t sequencerChannel = kChannelCount;
        /** Staged pattern of sequencerChannel (the spare buffer, already clamped). */
        SequencerPatternV2* sequencerPattern = nullptr;
    };

    /** @brief Scheduler state a restart preparation depends on, taken under a short lock. */
    struct TempoSnapshot {
        /** scheduleGeneration_ when the snapshot was taken. */
        std::uint32_t generation = 0U;
        /** externalLocked_ when the snapshot was taken. */
        bool externalLocked = false;
        /** externalBpmMilli_ when the snapshot was taken, milli-BPM. */
        std::uint32_t externalBpmMilli = 0U;
    };

    /**
     * @brief Position-independent first event of every GLOBAL channel after a restart.
     *
     * A restart puts the epoch on the current master position P, so the next event of
     * a GLOBAL channel is P plus an offset that only the configuration and the tempo
     * determine. No default member initializers: prepareRestart() writes every field,
     * so the scheduler interrupt does not clear it byte by byte first.
     */
    struct PreparedRestart {
        /** First event of each GLOBAL channel relative to the new epoch, Q32; 0 for FREE. */
        std::uint64_t firstEventOffsetQ32[kChannelCount];
        /** Step phase of each GLOBAL channel at serial 0; value-initialized for FREE. */
        PreparedStepPhase phases[kChannelCount];
        /** Channels the restart reschedules (GLOBAL; bit n = channel n). */
        std::uint8_t resetMask;
    };
    static_assert(
        std::is_trivially_default_constructible<PreparedRestart>::value,
        "The scheduler interrupt must not clear a restart byte by byte");

    /** @brief Transport change that restarts the GLOBAL schedules. */
    enum class TransportRequest : std::uint8_t { Play, Stop, Reset };

    /** Preparations outside the lock before one staged swap computes under the lock. */
    static constexpr std::uint8_t kStagedSwapAttempts = 3U;

    /** @brief Copy of the scheduler state a preparation starts from, taken under a short lock. */
    struct ScheduleSnapshot {
        /** scheduleGeneration_ when the snapshot was taken. */
        std::uint32_t generation = 0U;
        /** masterPositionQ32_, Q32 master beats. */
        std::uint64_t positionQ32 = 0U;
        /** globalScheduleEpochQ32_, Q32 master beats. */
        std::uint64_t globalEpochQ32 = 0U;
        /** restartPending_. */
        bool restartPending = false;
        /** externalLocked_. */
        bool externalLocked = false;
        /** externalBpmMilli_, milli-BPM. */
        std::uint32_t externalBpmMilli = 0U;
        /** Tempo of the installed configuration in BPM; detects a manual change (D-02). */
        std::uint16_t configuredBpm = 0U;
        /** ChannelRuntime::scheduleEpochQ32 of each channel. */
        std::uint64_t epochQ32[kChannelCount]{};
        /** ChannelRuntime::nextEventSerial of each channel. */
        std::uint64_t nextEventSerial[kChannelCount]{};
        /** ChannelRuntime::lastEventSlotQ32 of each channel. */
        std::uint64_t lastEventSlotQ32[kChannelCount]{};
        /** ChannelRuntime::lastEventSlotValid of each channel. */
        bool lastEventSlotValid[kChannelCount]{};
    };

    /**
     * @brief Returns the tempo that currently drives the master timeline.
     * @return Tempo in milli-BPM.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    std::uint32_t effectiveBpmMilli() const;

    /**
     * @brief Returns the tempo that @p configuration and the external state select.
     * @param configuration Configuration whose source, loss mode and tempo apply.
     * @param externalLocked True when the external clock is locked.
     * @param externalBpmMilli External tempo in milli-BPM; 0 when none is known.
     * @return The external tempo for source EXTERNAL or AUTO when it is known and either
     *         locked or the loss mode is FREEWHEEL; otherwise the configured tempo; milli-BPM.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    static std::uint32_t effectiveBpmMilli(
        const EngineConfiguration& configuration,
        bool externalLocked,
        std::uint32_t externalBpmMilli);

    /**
     * @brief Builds the calculation inputs for @p configuration and the external state.
     * @param configuration Configuration the calculation reads.
     * @param customGrooveSpacingQ16 Humanize spacing cache of that configuration, one per
     *        channel.
     * @param grooves Custom Groove library and previews of that configuration.
     * @param externalLocked True when the external clock is locked.
     * @param externalBpmMilli External tempo in milli-BPM; 0 when none is known.
     * @return Inputs that refer to the arguments; they must outlive the inputs.
     *
     * @isr TIM3 scheduler interrupt: constant time (one 64-bit division), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    static ScheduleInputs makeScheduleInputs(
        const EngineConfiguration& configuration,
        const std::uint32_t* customGrooveSpacingQ16,
        const GrooveSources& grooves,
        bool externalLocked,
        std::uint32_t externalBpmMilli);

    /**
     * @brief Calculation inputs of the installed configuration at @p tempo (foreground).
     * @param tempo External lock state and tempo taken under a short lock.
     * @return Inputs that refer to the installed configuration, spacings and groove table.
     */
    ScheduleInputs foregroundScheduleInputs(const TempoSnapshot& tempo) const;

    /**
     * @brief Returns the calculation inputs of the installed configuration.
     * @return Inputs at the live external state; the caller holds the lock or is the ISR.
     *
     * @isr TIM3 scheduler interrupt: constant time (one 64-bit division), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    ScheduleInputs liveScheduleInputs() const;

    /**
     * @brief Returns the shortest representable musical interval at the scheduler cadence.
     * @return One scheduler tick in Q32 master beats at the current tempo, rounded up.
     *
     * @isr TIM3 scheduler interrupt: constant time (one 64-bit division), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    std::uint64_t minimumOutputIntervalQ32() const;

    /**
     * @brief Returns the shortest representable interval at @p bpmMilli and @p beatUnit.
     * @param bpmMilli Tempo in milli-BPM.
     * @param beatUnit Master meter note value; 0 is treated as 4.
     * @return One scheduler tick in Q32 master beats, rounded up; 1 when @p bpmMilli is 0.
     *
     * @isr TIM3 scheduler interrupt: constant time (one 64-bit division), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    static std::uint64_t minimumOutputIntervalQ32(std::uint32_t bpmMilli, std::uint8_t beatUnit);

    /**
     * @brief Resolves one channel's effective musical rate into a reduced fraction.
     * @param configuration Configuration that holds the channel.
     * @param channelIndex Zero-based channel index.
     * @param numerator Receives the numerator of the rate in events per master beat.
     * @param denominator Receives the denominator of that rate; both stay 1 for OFF.
     *
     * @isr TIM3 scheduler interrupt: bounded (two Euclidean GCD reductions), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    static void resolveChannelRate(
        const EngineConfiguration& configuration,
        std::size_t channelIndex,
        std::uint32_t& numerator,
        std::uint32_t& denominator);

    /**
     * @brief Nominal interval of one channel for the given calculation inputs.
     * @param inputs Calculation inputs.
     * @param channelIndex Zero-based channel index.
     * @return Interval in Q32 master beats, at least inputs.minimumIntervalQ32.
     *
     * @isr TIM3 scheduler interrupt: bounded, non-blocking, no allocation, opens no interrupt
     *      lock.
     */
    static std::uint64_t calculateNominalBaseIntervalQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex);

    /**
     * @brief Returns the exact cumulative base-grid offset for one event serial.
     * @param inputs Calculation inputs.
     * @param channelIndex Zero-based channel index.
     * @param eventSerial Event serial counted from the epoch.
     * @return Offset from the epoch in Q32 master beats, without phase, swing, groove or
     *         humanize; saturated at UINT64_MAX.
     *
     * @isr TIM3 scheduler interrupt: constant time (64-bit divisions), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    static std::uint64_t calculateBaseEventOffsetQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        std::uint64_t eventSerial);

    /**
     * @brief Returns the absolute epoch-anchored event position including phase, swing, and
     *        ONE CLOCK humanize.
     * @param channelIndex Zero-based channel index; the live inputs and channel epoch apply.
     * @param eventSerial Event serial counted from the epoch.
     * @param nominalSlotQ32 Receives the nominal grid slot (epoch plus base offset); nullptr
     *        to skip.
     * @return Event position in Q32 master beats, groove included; saturated at UINT64_MAX.
     *
     * @isr TIM3 scheduler interrupt: constant time (64-bit divisions), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    std::uint64_t calculateEventPositionQ32(
        std::size_t channelIndex,
        std::uint64_t eventSerial,
        std::uint64_t* nominalSlotQ32 = nullptr) const;

    /**
     * @brief Event position for the given inputs and channel epoch.
     * @param inputs Calculation inputs.
     * @param channelIndex Zero-based channel index.
     * @param scheduleEpochQ32 Epoch of the channel grid, Q32 master beats.
     * @param eventSerial Event serial counted from the epoch.
     * @param nominalSlotQ32 Receives the nominal grid slot (epoch plus base offset, no phase,
     *        swing, groove or humanize); nullptr to skip.
     * @return Event position in Q32 master beats with phase, swing, groove and humanize;
     *         never before the epoch plus phase; saturated at UINT64_MAX.
     *
     * @isr TIM3 scheduler interrupt: constant time (64-bit divisions), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    std::uint64_t calculateEventPositionQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        std::uint64_t scheduleEpochQ32,
        std::uint64_t eventSerial,
        std::uint64_t* nominalSlotQ32 = nullptr) const;

    /**
     * @brief Calculates one channel interval in real microseconds for gate-length limiting.
     * @param inputs Calculation inputs; a tempo of 0 is treated as 120 BPM.
     * @param channelIndex Zero-based channel index.
     * @return Interval in microseconds, at least config::kSchedulerTickUs.
     *
     * @isr TIM3 scheduler interrupt: constant time (one 64-bit division), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    static std::uint64_t calculateBaseIntervalUs(
        const ScheduleInputs& inputs,
        std::size_t channelIndex);

    /**
     * @brief Computes the Custom Groove spacing cache value for one channel configuration.
     * @param grooves Library and previews that will be active after the pending change.
     * @param channelIndex Zero-based channel whose preview state and library slot apply.
     * @param channel Channel configuration that will be active after the pending change.
     * @return Q16 spacing; kCustomGrooveSpacingQ16One when no Custom Groove is active.
     */
    static std::uint32_t calculateCustomGrooveSpacingQ16(
        const GrooveSources& grooves,
        std::size_t channelIndex,
        const ChannelConfig& channel);

    /**
     * @brief Returns the safely capped ONE CLOCK humanization amount for one output.
     * @param inputs Calculation inputs.
     * @param channelIndex Zero-based channel index.
     * @return Amount in microseconds: the configured value, capped at
     *         config::kMaximumHumanizeUs and at half the shortest shaped interval less one
     *         tick, in whole ticks; 0 outside ONE CLOCK mode.
     *
     * @isr TIM3 scheduler interrupt: constant time (64-bit divisions), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    std::uint16_t effectiveHumanizeUs(const ScheduleInputs& inputs, std::size_t channelIndex) const;

    /**
     * @brief Returns deterministic signed per-output timing jitter in Q32 master-beat units.
     * @param inputs Calculation inputs.
     * @param channelIndex Zero-based channel index.
     * @param eventSerial Event serial; serial 0 (the shared restart) gets no offset.
     * @return Whole scheduler ticks times inputs.minimumIntervalQ32, within
     *         +-effectiveHumanizeUs(); a stateless hash of serial and channel.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    std::int64_t calculateHumanizeOffsetQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        std::uint64_t eventSerial) const;

    /**
     * @brief Drives one output only when its logical state changes; staged inside a quantum.
     * @param channelIndex Zero-based channel index.
     * @param high True for HIGH. A rise of a gate that was HIGH at the last commit moves to
     *        the next tick (F-015).
     *
     * Outside a quantum (foreground under the lock) the driver pin is written at once.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock. Stages the gate change in the driver.
     */
    void setGateState(std::size_t channelIndex, bool high);

    /**
     * @brief setGateState(channel, false) for every channel; writes only the HIGH ones.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock.
     */
    void lowerAllGatesUnsafe();

    /**
     * @brief Scheduler body over @p elapsedTicks ticks, before the single gate write.
     * @param elapsedTicks Scheduler ticks this quantum covers; 0 does nothing.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels, at most eight events and seven
     *      ratchet sub-events per channel), non-blocking, no allocation, opens no interrupt
     *      lock.
     */
    void runSchedulerTick(std::uint32_t elapsedTicks);

    /**
     * @brief Emits rising edges that were moved by one tick in the previous quantum.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock.
     */
    void serviceDeferredGateRises();

    /**
     * @brief Ends every HIGH gate one tick before its next rising edge would reach it.
     * @param masterIncrementQ32 Smallest master advance of one tick, Q32 master beats; the
     *        next tick advances by this or by one unit more.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock.
     */
    void endGatesBeforeNextRise(std::uint64_t masterIncrementQ32);

    /**
     * @brief Returns the first serial whose grid slot follows the last processed event.
     * @param inputs Calculation inputs.
     * @param channelIndex Zero-based channel index.
     * @param basis Schedule basis with the last processed grid slot.
     * @param serial First serial that is still eligible by position.
     * @return @p serial or a later serial; each correction direction is limited to four
     *         steps after the estimate.
     */
    static std::uint64_t firstSerialAfterLastEvent(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        const ChannelScheduleBasis& basis,
        std::uint64_t serial);

    /**
     * @brief Evaluates and emits one scheduled event for one channel.
     * @param channelIndex Zero-based channel index.
     *
     * Applies pattern, mute and probability, sets the gate length, ties and ratchets.
     *
     * @isr TIM3 scheduler interrupt: constant time (64-bit divisions), non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    void fireChannelEvent(std::size_t channelIndex);

    /**
     * @brief Emits bounded pending EVEN ratchet sub-events from the shared master timeline.
     *
     * @isr TIM3 scheduler interrupt: at most seven sub-events per channel and quantum,
     *      non-blocking, no allocation, opens no interrupt lock.
     */
    void serviceSequencerRatchets();

    /**
     * @brief Rebuilds one channel on its deterministic epoch-anchored musical grid.
     * @param channelIndex Zero-based channel index.
     * @param includeCurrentBoundary True when an event exactly at the current position may
     *        still fire.
     *
     * Caller holds the lock. GLOBAL channels also re-derive their step phase.
     */
    void scheduleChannelFromCurrentPosition(
        std::size_t channelIndex,
        bool includeCurrentBoundary = false);

    /**
     * @brief Returns the live basis of one channel; caller holds the lock or is the ISR.
     * @param channelIndex Zero-based channel index.
     * @param includeCurrentBoundary True when an event exactly at the current position may
     *        fire; a pending restart allows it as well.
     * @return Basis at the current master position and the channel's epoch.
     */
    ChannelScheduleBasis liveScheduleBasis(std::size_t channelIndex, bool includeCurrentBoundary)
        const;

    /**
     * @brief Computes the next event of one channel from @p basis (64-bit divisions).
     * @param inputs Calculation inputs.
     * @param channelIndex Zero-based channel index.
     * @param basis Position, epoch and last processed slot to start from.
     * @return Schedule of the first eligible event that does not repeat a processed slot.
     *
     * Starts from a division estimate and corrects it by single serials.
     */
    PreparedChannelSchedule prepareChannelSchedule(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        const ChannelScheduleBasis& basis) const;

    /**
     * @brief Installs a prepared schedule; no division.
     * @param channelIndex Zero-based channel index.
     * @param schedule Schedule from prepareChannelSchedule().
     */
    void installChannelSchedule(std::size_t channelIndex, const PreparedChannelSchedule& schedule);

    /**
     * @brief Returns the active local cycle length used to derive GLOBAL step phase.
     * @param configuration Configuration that holds the channel.
     * @param channelIndex Zero-based channel index.
     * @return EUCLID steps, SEQ length or CLOCK meter beats; at least 1, 1 for OFF.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    static std::uint8_t channelCycleLength(
        const EngineConfiguration& configuration,
        std::size_t channelIndex);

    /**
     * @brief Re-establishes GLOBAL pattern phase or bounds FREE local phase after edits.
     * @param channelIndex Zero-based channel index.
     */
    void synchronizeChannelStepPhase(std::size_t channelIndex);

    /**
     * @brief Computes the step phase that @p nextEventSerial implies under @p configuration.
     * @param configuration Configuration that will be installed.
     * @param channelIndex Zero-based channel.
     * @param nextEventSerial Serial of the channel's next event.
     * @param sequencerPattern Active Sequencer 2.0 pattern of the channel, nullptr if none.
     * @return Phase derived from the serial for a Sequencer 2.0 or GLOBAL channel; otherwise
     *         a phase that keeps the FREE local step.
     *
     * @isr TIM3 scheduler interrupt: constant time (two traversals, one 64-bit modulo),
     *      non-blocking, no allocation, opens no interrupt lock.
     */
    PreparedStepPhase prepareStepPhase(
        const EngineConfiguration& configuration,
        std::size_t channelIndex,
        std::uint64_t nextEventSerial,
        const SequencerPatternV2* sequencerPattern) const;

    /**
     * @brief Installs a staged Sequencer 2.0 pattern; the old buffer becomes the spare.
     * @param channelIndex Zero-based channel index.
     * @param pattern Staged, clamped pattern in the former spare buffer.
     *
     * Caller holds the lock. Ends a held tie and drops pending ratchets of the channel.
     */
    void installSequencerPatternUnsafe(std::size_t channelIndex, SequencerPatternV2* pattern);

    /**
     * @brief Returns the installed Sequencer 2.0 pattern of a channel, nullptr if inactive.
     * @param channelIndex Zero-based channel index.
     * @return Installed pattern, or nullptr before the first installation.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    const SequencerPatternV2* liveSequencerPattern(std::size_t channelIndex) const;

    /**
     * @brief Installs a prepared step phase under the installed configuration.
     * @param channelIndex Zero-based channel index.
     * @param phase Phase from prepareStepPhase(); a FREE phase only bounds the local step.
     */
    void installStepPhase(std::size_t channelIndex, const PreparedStepPhase& phase);

    /**
     * @brief Installs @p staged; prepares the schedules of @p channelMask outside the lock.
     * @param staged Configuration and spacings to install.
     * @param channelMask Channels whose gate, schedule and step phase follow the change.
     * @param rescheduleChannels True to rebuild the next event of every channel in @p channelMask.
     */
    void swapConfiguration(
        const StagedConfiguration& staged,
        std::uint8_t channelMask,
        bool rescheduleChannels);

    /**
     * @brief Returns the configuration buffer that is not installed (foreground only).
     * @return The buffer the scheduler does not read.
     */
    EngineConfiguration* inactiveConfiguration();

    /**
     * @brief Copies the installed groove table into the inactive one; returns it (foreground).
     * @return The staged table; no pattern is copied, only the pointers and preview values.
     */
    GrooveSources* stageGrooveSources();

    /**
     * @brief A staged swap of the live configuration with @p grooves and the live spacings.
     * @param grooves Groove table the swap installs.
     * @return Staged swap that keeps the installed configuration buffer.
     */
    StagedConfiguration stageLiveConfiguration(GrooveSources* grooves) const;

    /**
     * @brief Copies the scheduler state into @p snapshot; caller holds the lock.
     * @param snapshot Receives the schedule generation, master and channel state.
     */
    void takeScheduleSnapshotUnsafe(ScheduleSnapshot& snapshot) const;

    /**
     * @brief Prepares schedules and step phases of @p channelMask from @p snapshot.
     * @param staged Configuration, spacings, grooves and pattern that the swap installs.
     * @param snapshot Scheduler state the preparation starts from.
     * @param channelMask Channels to prepare (bit n = channel n).
     * @param rescheduleChannels True to compute a new schedule; false keeps the serial.
     * @param schedules Receives the schedule of each prepared channel.
     * @param phases Receives the step phase of each prepared channel.
     */
    void prepareConfigurationSwap(
        const StagedConfiguration& staged,
        const ScheduleSnapshot& snapshot,
        std::uint8_t channelMask,
        bool rescheduleChannels,
        PreparedChannelSchedule (&schedules)[kChannelCount],
        PreparedStepPhase (&phases)[kChannelCount]) const;

    /**
     * @brief True when the scheduler has not moved a basis of @p channelMask since @p snapshot.
     * @param snapshot Snapshot the preparation used.
     * @param channelMask Channels whose next serial must be unchanged (bit n = channel n).
     * @return False when the schedule generation or a next serial in the mask changed.
     */
    bool scheduleSnapshotCurrentUnsafe(const ScheduleSnapshot& snapshot, std::uint8_t channelMask)
        const;

    /**
     * @brief Installs a prepared swap; caller holds the lock.
     * @param staged Configuration, spacings, grooves and pattern to install.
     * @param channelMask Channels whose gate, schedule and step phase follow the change.
     * @param rescheduleChannels True to install the prepared schedules.
     * @param schedules Prepared schedule of each channel in @p channelMask.
     * @param phases Prepared step phase of each channel in @p channelMask.
     */
    void installConfigurationSwapUnsafe(
        const StagedConfiguration& staged,
        std::uint8_t channelMask,
        bool rescheduleChannels,
        const PreparedChannelSchedule (&schedules)[kChannelCount],
        const PreparedStepPhase (&phases)[kChannelCount]);

#ifdef CLOCK_HOST_TEST
    /** @brief Counts one schedule calculation if it runs under an InterruptLock (host tests). */
    void countCalculationUnderLockForTest() const;
#endif

    /**
     * @brief Returns the swing displacement used by the epoch-anchored event grid.
     * @param inputs Calculation inputs.
     * @param channelIndex Zero-based channel index; swing above 50 % is treated as 50 %.
     * @param baseIntervalQ32 Nominal interval, Q32 master beats.
     * @return Delay of an odd event in Q32 master beats; it leaves at least one scheduler
     *         tick of the interval; 0 without swing.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    static std::uint64_t calculateSwingDeltaQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        std::uint64_t baseIntervalQ32);

    /**
     * @brief Applies one external reference without manipulating global interrupt state.
     * @param bpmMilli Filtered external tempo in milli-BPM.
     * @param pulsesPerQuarterNote Input pulses per quarter note; 0 is treated as 1.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock. The first pulse after lock and the end of a count-in
     *      compute the restart schedules in place (64-bit divisions).
     */
    void acceptExternalPulseUnsafe(std::uint32_t bpmMilli, std::uint8_t pulsesPerQuarterNote);

    /**
     * @brief Shifts pending GLOBAL schedules by an observed external phase correction.
     * @param expectedPulsePositionQ32 Master position where the pulse was expected, Q32.
     *
     * Only moves positions forward (F-069): a late pulse moves the GLOBAL grid later, an
     * early one moves the master timeline and the FREE schedules later.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock.
     */
    void alignGlobalSchedulesToExternalPulse(std::uint64_t expectedPulsePositionQ32);

    /**
     * @brief Resets master/runtime state while respecting each channel's GLOBAL/FREE policy.
     *
     * Caller holds the lock or is the scheduler interrupt; prepares and installs at once.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock. Computes the restart schedules in place (64-bit divisions).
     */
    void resetRuntime();

    /**
     * @brief Copies the state a restart preparation depends on; caller holds the lock.
     * @return Schedule generation and external lock state and tempo.
     */
    TempoSnapshot takeTempoSnapshotUnsafe() const;

    /**
     * @brief Computes the first event offset and step phase of every GLOBAL channel.
     * @param inputs Calculation inputs.
     * @param restart Receives every field; FREE channels get offset 0 and an empty phase.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock; 64-bit divisions.
     */
    void prepareRestart(const ScheduleInputs& inputs, PreparedRestart& restart) const;

    /**
     * @brief Restarts master and GLOBAL schedules at the current position; no division.
     * @param restart Restart from prepareRestart().
     *
     * Lowers every gate and advances the schedule generation.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), no division, non-blocking, no
     *      allocation, opens no interrupt lock.
     */
    void installRestartUnsafe(const PreparedRestart& restart);

    /**
     * @brief Applies @p request with the restart schedules prepared outside the lock (F-072).
     *
     * A preparation is current while the schedule generation is unchanged; otherwise
     * it repeats, and after kStagedSwapAttempts the request computes under the lock.
     *
     * @param request Transport change.
     */
    void applyTransportRequest(TransportRequest request);

    /**
     * @brief applyTransportRequest() that applies only if @p condition agrees under the lock.
     * @param request Transport change.
     * @param condition Called once under the installing lock; nullptr always applies.
     * @param context Passed to @p condition.
     * @return False when @p condition declined.
     */
    bool applyTransportRequestWhen(
        TransportRequest request,
        StartCondition condition,
        void* context);

    /**
     * @brief Calls @p condition and applies @p request if it agrees; caller holds the lock.
     * @param request Transport change.
     * @param restart Prepared restart, or nullptr to compute it under the lock.
     * @param condition nullptr always applies.
     * @param context Passed to @p condition.
     * @return False when @p condition declined.
     */
    bool applyTransportRequestIfUnsafe(
        TransportRequest request,
        const PreparedRestart* restart,
        StartCondition condition,
        void* context);

    /**
     * @brief Applies @p request; caller holds the lock.
     * @param request Transport change.
     * @param restart Prepared restart, or nullptr to compute it under the lock.
     *
     * @isr TIM3 scheduler interrupt (through playFromIsr() and stopFromIsr(), with nullptr):
     *      bounded (eight channels), non-blocking, no allocation, opens no interrupt lock;
     *      the restart is computed in place (64-bit divisions).
     */
    void applyTransportRequestUnsafe(TransportRequest request, const PreparedRestart* restart);

    /**
     * @brief True when @p request will restart the GLOBAL schedules (pre-check without lock).
     * @param request Transport change.
     * @return True for STOP and RESET, and for a fresh PLAY without count-in.
     */
    bool transportRequestRestarts(TransportRequest request) const;

    /** @brief Counts one staged swap that computed under the lock (test and debug counters). */
    void countStagedSwapFallback();

    /**
     * @brief Arms the silent STOP->PLAY count-in from the configured master-beat count.
     *
     * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    void startPreCountUnsafe();

    /**
     * @brief Finishes the count-in and restarts musical phase at the first audible boundary.
     *
     * @isr TIM3 scheduler interrupt: bounded (eight channels), non-blocking, no allocation,
     *      opens no interrupt lock. Computes the restart schedules in place (64-bit divisions).
     */
    void finishPreCountUnsafe();

    /**
     * @brief Drops the AUTO freewheel tempo after a manual tempo change while unlocked; caller
     *        holds the lock.
     */
    void discardFreewheelTempoAfterManualChangeUnsafe();

    /**
     * @brief Copies master and channel configuration without entering a critical section.
     * @param state Application state resolved into the installed configuration buffer.
     */
    void copyConfigurationUnsafe(const ClockState& state);

    /** @brief Points every groove, pattern and expression table at its own buffer. */
    void initializeBufferTables();

    /**
     * @brief Resolves @p state into an engine configuration (no shared state).
     * @param state Application state.
     * @param configuration Receives the master values and the resolved physical outputs.
     */
    static void resolveConfiguration(const ClockState& state, EngineConfiguration& configuration);

    /**
     * @brief Computes the Custom Groove spacing cache for every channel of one state (foreground).
     * @param state Application state that the following configuration swap installs.
     * @param spacingQ16 Receives one Q16 spacing per physical output.
     */
    void calculateCustomGrooveSpacingsQ16(
        const ClockState& state,
        std::uint32_t (&spacingQ16)[kChannelCount]) const;

    /**
     * @brief Installs precomputed spacings; caller holds the configuration-swap critical
     *        section.
     * @param spacingQ16 One Q16 spacing per physical output.
     */
    void installCustomGrooveSpacingsUnsafe(const std::uint32_t (&spacingQ16)[kChannelCount]);

    /** Gate output driver; every gate GPIO change goes through it. */
    hal::GateOutputDriver& gateOutputs_;
    /**
     * Two configuration buffers (W-060). The scheduler reads *configuration_; the
     * foreground stages the other buffer outside the lock and installs it by
     * swapping this pointer under the lock.
     */
    EngineConfiguration configurationBuffers_[2]{};
    /** Installed configuration the scheduler reads; swapped under the lock. */
    EngineConfiguration* configuration_ = &configurationBuffers_[0];
    /** True while the scheduler runs, including the count-in. */
    volatile bool playing_ = true;
    /** True after a restart until the first played quantum; PLAY then starts fresh. */
    volatile bool restartPending_ = true;
    /** True while the count-in runs. */
    volatile bool preCountActive_ = false;
    /** Count-in beats still to go. */
    volatile std::uint8_t preCountRemaining_ = 0U;
    /** Elapsed part of the current count-in beat, Q32. */
    volatile std::uint64_t preCountPhaseQ32_ = 0U;
    /** Division remainder of the internal count-in advance. */
    volatile std::uint64_t preCountRemainder_ = 0U;
    /** Division remainder of the count-in advance by external pulses. */
    volatile std::uint32_t preCountExternalPulseRemainder_ = 0U;
    /** Master timeline position, Q32 master beats; stands while stopped or paused. */
    volatile std::uint64_t masterPositionQ32_ = 0U;
    /** Epoch of the GLOBAL channel grids, Q32; set to the master position at a restart. */
    volatile std::uint64_t globalScheduleEpochQ32_ = 0U;
    /** Elapsed part of the current master beat, Q32. */
    volatile std::uint64_t masterBeatPhaseQ32_ = 0U;
    /** Completed master beats since the last restart; wraps at 2^32. */
    volatile std::uint32_t masterBeatSerial_ = 0U;
    /** Current beat in the bar, 1..beats per bar. */
    volatile std::uint8_t masterBeat_ = 1U;
    /** Current bar, 1..999. */
    volatile std::uint16_t masterBar_ = 1U;
    /** True while the external clock is locked. */
    volatile bool externalLocked_ = false;
    /** True while a level-sensitive external reset holds the engine. */
    volatile bool externalResetGate_ = false;
    /** External tempo in milli-BPM; 0 when none is known. */
    volatile std::uint32_t externalBpmMilli_ = 0U;
    /** True once a pulse established the external epoch; cleared when the lock is lost. */
    volatile bool externalPhaseInitialized_ = false;
    /** Master position at the last accepted external pulse, Q32. */
    volatile std::uint64_t externalReferenceQ32_ = 0U;
    /** Position counted from external pulses since the last restart, Q32 (bar/beat). */
    volatile std::uint64_t externalMusicalPositionQ32_ = 0U;
    /** Division remainder of the external pulse interval. */
    volatile std::uint32_t externalPulseRemainder_ = 0U;
    /** Scheduling state of each channel. */
    ChannelRuntime channelRuntime_[kChannelCount]{};
    /** Scheduler ticks counted since start; wraps at 2^32. */
    volatile std::uint32_t schedulerTickCounter_ = 0U;
    /**
     * Incremented whenever scheduler context moves a schedule basis other than by
     * processing an event: restart, phase correction, external tempo or lock.
     */
    volatile std::uint32_t scheduleGeneration_ = 0U;
    /** Division remainder of the master advance; reset by a restart. */
    volatile std::uint64_t masterRemainder_ = 0U;
    /** TIM5 timestamp of the last counted TIM3 update (scheduler context only, F-012). */
    std::uint32_t lastSchedulerUpdateUs_ = 0U;
    /** False until the first scheduler interrupt anchored lastSchedulerUpdateUs_. */
    bool schedulerUpdateAnchored_ = false;
    /** True inside a scheduler quantum; gate changes are staged meanwhile. */
    bool schedulerQuantumOpen_ = false;
    /** Gates the engine holds HIGH (bit n = channel n). */
    std::uint8_t gateHighMask_ = 0U;
    /** gateHighMask_ at the last gate write; such a gate cannot rise in the next quantum. */
    std::uint8_t gatesHighAtLastCommit_ = 0U;
    /** Gates raised in the open quantum (bit n = channel n). */
    std::uint8_t gatesRaisedThisQuantum_ = 0U;
    /** Channels whose rising edge moved to the next tick (bit n = channel n). */
    std::uint8_t deferredRiseMask_ = 0U;
#ifdef CLOCK_HOST_TEST
    std::uint32_t resetRuntimeCountForTest_ = 0U;
    std::uint32_t deferredGateRiseCountForTest_ = 0U;
    std::uint32_t schedulerCatchUpLimitCountForTest_ = 0U;
    std::uint32_t configurationSwapFallbackCountForTest_ = 0U;
    mutable std::uint32_t scheduleCalculationsUnderLockForTest_ = 0U;
    ConfigurationSwapHookForTest configurationSwapHookForTest_ = nullptr;
    void* configurationSwapHookContextForTest_ = nullptr;
#endif
};

}  // namespace clockfw::engine
