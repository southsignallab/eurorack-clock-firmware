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
    bool playing = false;
    std::uint64_t masterPositionQ32 = 0U;
    std::uint64_t masterBeatPhaseQ32 = 0U;
    std::uint32_t masterBeatSerial = 0U;
    std::uint8_t masterBeat = 1U;
    std::uint16_t masterBar = 1U;
    bool preCountActive = false;
    std::uint8_t preCountRemaining = 0U;
    std::uint64_t preCountPhaseQ32 = 0U;
    bool externalLocked = false;
    bool externalResetHeld = false;
    std::uint32_t externalBpmMilli = 0U;
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
    void updateSequencerStepMetadata(
        std::size_t channelIndex,
        const StepMetadataTable& metadata);

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

    /** @brief Applies one non-persistent Custom Groove preview to a runtime channel. */
    void setCustomGroovePreview(
        std::size_t channelIndex,
        const CustomGroovePattern& pattern,
        std::uint8_t amountPercent = 100U,
        std::uint8_t rotation = 0U,
        bool rescheduleChannel = true);

    /** @brief Clears a runtime-only Custom Groove preview from one channel. */
    void clearCustomGroovePreview(
        std::size_t channelIndex,
        bool rescheduleChannel = true);

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
     */
    void playFromIsr();

    /** @brief Interrupt variant of stop(): constructs no InterruptLock (F-030), see playFromIsr(). */
    void stopFromIsr();

    /** @brief Decides under the lock whether a prepared start still applies; gets the caller's context. */
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
     */
    void resetGlobalPhaseFromIsr();

    /**
     * @brief Updates the external synchronization lock information.
     * @param locked True when the external clock estimator is locked.
     * @param bpmMilli Smoothed external tempo in milli-BPM.
     */
    void setExternalLock(bool locked, std::uint32_t bpmMilli);

    /** @brief ISR-safe external lock update used by the real-time input controller. */
    void setExternalLockFromIsr(bool locked, std::uint32_t bpmMilli);

    /** @brief Applies or releases a level-sensitive external reset hold from scheduler context. */
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
     */
    void acceptExternalPulseFromIsr(
        std::uint32_t bpmMilli,
        std::uint8_t pulsesPerQuarterNote);

    /** @brief ISR-only: opens the quantum whose gate changes processSchedulerTick() writes once. */
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
     */
    std::uint32_t elapsedSchedulerTicksFromIsr(std::uint32_t updateTimestampUs);

    /**
     * @brief Executes one deterministic scheduler quantum and writes its gate edges once.
     * @param elapsedTicks Scheduler ticks this quantum covers (see elapsedSchedulerTicksFromIsr()).
     *
     * This method is ISR-only and must remain bounded, allocation-free, and non-blocking.
     */
    void processSchedulerTick(std::uint32_t elapsedTicks = 1U);

    /** @brief Returns interrupt-safe snapshot of the timing state used by the UI. */
    EngineSnapshot snapshot() const;

    /**
     * @brief Returns whether the scheduler is running, including an active pre-count.
     *
     * Read under an interrupt lock. The scheduler ISR can start the engine on its
     * own (RUN level, START/RESTART input, lock acquisition), so this is the
     * authority for real-time safety decisions, not the foreground transport field.
     */
    bool isPlaying() const;

    /** @brief Returns the selected output's nominal event interval in Q32 master-beat units. */
    std::uint64_t nominalIntervalQ32(std::size_t channelIndex) const;

    /**
     * @brief Returns the position of event serial 0 of a channel: schedule epoch plus phase.
     *
     * Groove Record aligns its grid to it (F-022). The epoch is read under a short lock;
     * the phase offset is computed after it.
     */
    std::uint64_t channelGridOriginQ32(std::size_t channelIndex) const;

    /**
     * @brief Estimates the engine position at an earlier microsecond timestamp.
     *
     * This compensates front-panel debounce latency for Groove Record without
     * changing normal button debounce semantics. The estimate is quantized to
     * the 20 kHz scheduler cadence and bounded to recent timestamps.
     */
    std::uint64_t estimateMasterPositionAtUs(
        std::uint32_t eventTimestampUs,
        std::uint32_t nowUs) const;

#ifdef CLOCK_HOST_TEST
    /** @brief Returns how many full runtime resets were executed in deterministic host tests. */
    std::uint32_t resetRuntimeCountForTest() const;
    /** @brief Returns the channels the engine currently holds HIGH (bit n = channel n). */
    std::uint8_t gateHighMaskForTest() const { return gateHighMask_; }
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

    /** @brief Returns the engine's 128-step expression snapshot of one channel. */
    const StepMetadataTable& sequencerStepMetadataForTest(std::size_t channelIndex) const {
        return *sequencerStepMetadata_[channelIndex < kChannelCount ? channelIndex : 0U];
    }
#endif

private:
    /** @brief Real-time copy of configuration values needed inside the ISR. */
    struct EngineConfiguration {
        std::uint16_t bpm = 120U;
        std::uint8_t beatsPerBar = 4U;
        std::uint8_t beatUnit = 4U;
        std::uint8_t preCountSteps = 0U;
        ClockSource source = ClockSource::Internal;
        SyncLossMode syncLossMode = SyncLossMode::Freewheel;
        OperatingMode operatingMode = OperatingMode::Independent;
        std::uint16_t unifiedHumanizeUs = 0U;
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
        std::uint8_t previewAmount[kChannelCount]{};
        std::uint8_t previewRotation[kChannelCount]{};
        bool previewActive[kChannelCount]{};
    };

    /** Library slots, one preview per channel and one spare buffer. */
    static constexpr std::size_t kCustomGrooveBufferCount =
        static_cast<std::size_t>(kCustomGrooveSlotCount) + kChannelCount + 1U;
    CustomGroovePattern customGrooveBuffers_[kCustomGrooveBufferCount]{};
    /** The pattern buffer no table refers to; only the foreground writes it. */
    CustomGroovePattern* spareCustomGroove_ = nullptr;
    GrooveSources grooveSourceBuffers_[2]{};
    GrooveSources* grooveSources_ = &grooveSourceBuffers_[0];

    /** One pattern per channel and one spare; the lock swaps pointers (F-072). */
    SequencerPatternV2 sequencerPatternBuffers_[kChannelCount + 1U]{};
    SequencerPatternV2* sequencerPatterns_[kChannelCount]{};
    SequencerPatternV2* spareSequencerPattern_ = nullptr;
    /** One expression table per channel and one spare; the lock swaps pointers (F-071). */
    StepMetadataTable sequencerStepMetadataBuffers_[kChannelCount + 1U]{};
    StepMetadataTable* sequencerStepMetadata_[kChannelCount]{};
    StepMetadataTable* spareSequencerStepMetadata_ = nullptr;
    bool sequencerPatternV2Active_[kChannelCount]{};
    /**
     * @brief Cached smallest adjacent Custom Groove event spacing per channel (Q16 of one interval).
     *
     * Computed in the foreground whenever the channel configuration, a library slot, or a
     * preview changes, and installed inside the same critical section as that change.
     * The scheduler only reads it; it is tempo-independent. Zero (no Humanize headroom)
     * until begin() installs the first configuration.
     */
    std::uint32_t customGrooveSpacingQ16_[kChannelCount]{};

    /** @brief Mutable scheduling state for one output channel. */
    struct ChannelRuntime {
        std::uint64_t nextEventQ32 = 0U;
        std::uint64_t scheduleEpochQ32 = 0U;
        std::uint64_t nextEventSerial = 0U;
        std::uint8_t step = 0U;
        std::uint8_t displayedStep = 0U;
        std::uint32_t gateOffTick = 0U;
        bool gateOffScheduled = false;
        bool gateHigh = false;
        bool lastEventSlotValid = false;
        bool tieHold = false;
        std::uint64_t nextEventSlotQ32 = 0U;
        std::uint64_t lastEventSlotQ32 = 0U;
        std::uint8_t ratchetRemaining = 0U;
        std::uint64_t nextRatchetQ32 = 0U;
        std::uint64_t ratchetSpacingQ32 = 0U;
        std::uint32_t ratchetGateTicks = 1U;
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
        std::uint64_t positionQ32 = 0U;
        std::uint64_t epochQ32 = 0U;
        std::uint64_t lastEventSlotQ32 = 0U;
        bool lastEventSlotValid = false;
        bool mayUseCurrentBoundary = false;
    };

    /**
     * @brief A computed channel schedule; installing it needs no division.
     *
     * Trivially constructible like PreparedStepPhase: the producers write every
     * field, so no lock or interrupt path clears it byte by byte first.
     */
    struct PreparedChannelSchedule {
        std::uint64_t epochQ32;
        std::uint64_t nextEventSerial;
        std::uint64_t nextEventQ32;
        std::uint64_t nextEventSlotQ32;
    };
    static_assert(std::is_trivially_default_constructible<PreparedChannelSchedule>::value,
                  "A schedule must not be cleared byte by byte under the lock");

    /**
     * @brief A computed pattern step phase; FREE channels are bounded at install.
     *
     * No default member initializers, so PreparedRestart stays trivially constructible
     * in the scheduler interrupt; every user value-initializes it with {}.
     */
    struct PreparedStepPhase {
        bool derivedFromSerial;
        std::uint8_t step;
        std::uint8_t displayedStep;
    };
    static_assert(std::is_trivially_default_constructible<PreparedStepPhase>::value,
                  "A restart must not clear its prepared step phases byte by byte");

    /** @brief Configuration, groove table, pattern and Humanize spacings that one swap installs. */
    struct StagedConfiguration {
        /** The buffer the scheduler does not read (inactiveConfiguration()), or the live one. */
        EngineConfiguration* configuration = nullptr;
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
        std::uint32_t generation = 0U;
        bool externalLocked = false;
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
        std::uint64_t firstEventOffsetQ32[kChannelCount];
        PreparedStepPhase phases[kChannelCount];
        /** Channels the restart reschedules (GLOBAL; bit n = channel n). */
        std::uint8_t resetMask;
    };
    static_assert(std::is_trivially_default_constructible<PreparedRestart>::value,
                  "The scheduler interrupt must not clear a restart byte by byte");

    /** @brief Transport change that restarts the GLOBAL schedules. */
    enum class TransportRequest : std::uint8_t { Play, Stop, Reset };

    /** Preparations outside the lock before one staged swap computes under the lock. */
    static constexpr std::uint8_t kStagedSwapAttempts = 3U;

    /** @brief Copy of the scheduler state a preparation starts from, taken under a short lock. */
    struct ScheduleSnapshot {
        std::uint32_t generation = 0U;
        std::uint64_t positionQ32 = 0U;
        std::uint64_t globalEpochQ32 = 0U;
        bool restartPending = false;
        bool externalLocked = false;
        std::uint32_t externalBpmMilli = 0U;
        std::uint16_t configuredBpm = 0U;
        std::uint64_t epochQ32[kChannelCount]{};
        std::uint64_t nextEventSerial[kChannelCount]{};
        std::uint64_t lastEventSlotQ32[kChannelCount]{};
        bool lastEventSlotValid[kChannelCount]{};
    };

    /** @brief Returns the tempo that currently drives the master timeline. */
    std::uint32_t effectiveBpmMilli() const;

    /** @brief Returns the tempo that @p configuration and the external state select. */
    static std::uint32_t effectiveBpmMilli(
        const EngineConfiguration& configuration,
        bool externalLocked,
        std::uint32_t externalBpmMilli);

    /** @brief Builds the calculation inputs for @p configuration and the external state. */
    static ScheduleInputs makeScheduleInputs(
        const EngineConfiguration& configuration,
        const std::uint32_t* customGrooveSpacingQ16,
        const GrooveSources& grooves,
        bool externalLocked,
        std::uint32_t externalBpmMilli);

    /** @brief Calculation inputs of the installed configuration at @p tempo (foreground). */
    ScheduleInputs foregroundScheduleInputs(const TempoSnapshot& tempo) const;

    /** @brief Returns the calculation inputs of the installed configuration. */
    ScheduleInputs liveScheduleInputs() const;

    /** @brief Returns the shortest representable musical interval at the scheduler cadence. */
    std::uint64_t minimumOutputIntervalQ32() const;

    /** @brief Returns the shortest representable interval at @p bpmMilli and @p beatUnit. */
    static std::uint64_t minimumOutputIntervalQ32(std::uint32_t bpmMilli, std::uint8_t beatUnit);

    /** @brief Resolves one channel's effective musical rate into a reduced fraction. */
    static void resolveChannelRate(
        const EngineConfiguration& configuration,
        std::size_t channelIndex,
        std::uint32_t& numerator,
        std::uint32_t& denominator);

    /** @brief Nominal interval of one channel for the given calculation inputs. */
    static std::uint64_t calculateNominalBaseIntervalQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex);

    /** @brief Returns the exact cumulative base-grid offset for one event serial. */
    static std::uint64_t calculateBaseEventOffsetQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        std::uint64_t eventSerial);

    /** @brief Returns the absolute epoch-anchored event position including phase, swing, and ONE CLOCK humanize. */
    std::uint64_t calculateEventPositionQ32(
        std::size_t channelIndex,
        std::uint64_t eventSerial,
        std::uint64_t* nominalSlotQ32 = nullptr) const;

    /** @brief Event position for the given inputs and channel epoch. */
    std::uint64_t calculateEventPositionQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        std::uint64_t scheduleEpochQ32,
        std::uint64_t eventSerial,
        std::uint64_t* nominalSlotQ32 = nullptr) const;

    /** @brief Calculates one channel interval in real microseconds for gate-length limiting. */
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

    /** @brief Returns the safely capped ONE CLOCK humanization amount for one output. */
    std::uint16_t effectiveHumanizeUs(const ScheduleInputs& inputs, std::size_t channelIndex) const;

    /** @brief Returns deterministic signed per-output timing jitter in Q32 master-beat units. */
    std::int64_t calculateHumanizeOffsetQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        std::uint64_t eventSerial) const;

    /** @brief Drives one output only when its logical state changes; staged inside a quantum. */
    void setGateState(std::size_t channelIndex, bool high);

    /** @brief setGateState(channel, false) for every channel; writes only the HIGH ones. */
    void lowerAllGatesUnsafe();

    /** @brief Scheduler body over @p elapsedTicks ticks, before the single gate write. */
    void runSchedulerTick(std::uint32_t elapsedTicks);

    /** @brief Emits rising edges that were moved by one tick in the previous quantum. */
    void serviceDeferredGateRises();

    /** @brief Ends every HIGH gate one tick before its next rising edge would reach it. */
    void endGatesBeforeNextRise(std::uint64_t masterIncrementQ32);

    /** @brief Returns the first serial whose grid slot follows the last processed event. */
    static std::uint64_t firstSerialAfterLastEvent(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        const ChannelScheduleBasis& basis,
        std::uint64_t serial);

    /** @brief Evaluates and emits one scheduled event for one channel. */
    void fireChannelEvent(std::size_t channelIndex);

    /** @brief Emits bounded pending EVEN ratchet sub-events from the shared master timeline. */
    void serviceSequencerRatchets();

    /** @brief Rebuilds one channel on its deterministic epoch-anchored musical grid. */
    void scheduleChannelFromCurrentPosition(
        std::size_t channelIndex,
        bool includeCurrentBoundary = false);

    /** @brief Returns the live basis of one channel; caller holds the lock or is the ISR. */
    ChannelScheduleBasis liveScheduleBasis(
        std::size_t channelIndex,
        bool includeCurrentBoundary) const;

    /** @brief Computes the next event of one channel from @p basis (64-bit divisions). */
    PreparedChannelSchedule prepareChannelSchedule(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        const ChannelScheduleBasis& basis) const;

    /** @brief Installs a prepared schedule; no division. */
    void installChannelSchedule(std::size_t channelIndex, const PreparedChannelSchedule& schedule);

    /** @brief Returns the active local cycle length used to derive GLOBAL step phase. */
    static std::uint8_t channelCycleLength(
        const EngineConfiguration& configuration,
        std::size_t channelIndex);

    /** @brief Re-establishes GLOBAL pattern phase or bounds FREE local phase after edits. */
    void synchronizeChannelStepPhase(std::size_t channelIndex);

    /**
     * @brief Computes the step phase that @p nextEventSerial implies under @p configuration.
     * @param configuration Configuration that will be installed.
     * @param channelIndex Zero-based channel.
     * @param nextEventSerial Serial of the channel's next event.
     * @param sequencerPattern Active Sequencer 2.0 pattern of the channel, nullptr if none.
     */
    PreparedStepPhase prepareStepPhase(
        const EngineConfiguration& configuration,
        std::size_t channelIndex,
        std::uint64_t nextEventSerial,
        const SequencerPatternV2* sequencerPattern) const;

    /** @brief Installs a staged Sequencer 2.0 pattern; the old buffer becomes the spare. */
    void installSequencerPatternUnsafe(std::size_t channelIndex, SequencerPatternV2* pattern);

    /** @brief Returns the installed Sequencer 2.0 pattern of a channel, nullptr if inactive. */
    const SequencerPatternV2* liveSequencerPattern(std::size_t channelIndex) const;

    /** @brief Installs a prepared step phase under the installed configuration. */
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

    /** @brief Returns the configuration buffer that is not installed (foreground only). */
    EngineConfiguration* inactiveConfiguration();

    /** @brief Copies the installed groove table into the inactive one; returns it (foreground). */
    GrooveSources* stageGrooveSources();

    /** @brief A staged swap of the live configuration with @p grooves and the live spacings. */
    StagedConfiguration stageLiveConfiguration(GrooveSources* grooves) const;

    /** @brief Copies the scheduler state into @p snapshot; caller holds the lock. */
    void takeScheduleSnapshotUnsafe(ScheduleSnapshot& snapshot) const;

    /** @brief Prepares schedules and step phases of @p channelMask from @p snapshot. */
    void prepareConfigurationSwap(
        const StagedConfiguration& staged,
        const ScheduleSnapshot& snapshot,
        std::uint8_t channelMask,
        bool rescheduleChannels,
        PreparedChannelSchedule (&schedules)[kChannelCount],
        PreparedStepPhase (&phases)[kChannelCount]) const;

    /** @brief True when the scheduler has not moved a basis of @p channelMask since @p snapshot. */
    bool scheduleSnapshotCurrentUnsafe(
        const ScheduleSnapshot& snapshot,
        std::uint8_t channelMask) const;

    /** @brief Installs a prepared swap; caller holds the lock. */
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

    /** @brief Returns the swing displacement used by the epoch-anchored event grid. */
    static std::uint64_t calculateSwingDeltaQ32(
        const ScheduleInputs& inputs,
        std::size_t channelIndex,
        std::uint64_t baseIntervalQ32);

    /** @brief Applies one external reference without manipulating global interrupt state. */
    void acceptExternalPulseUnsafe(
        std::uint32_t bpmMilli,
        std::uint8_t pulsesPerQuarterNote);

    /** @brief Shifts pending GLOBAL schedules by an observed external phase correction. */
    void alignGlobalSchedulesToExternalPulse(std::uint64_t expectedPulsePositionQ32);

    /** @brief Resets master/runtime state while respecting each channel's GLOBAL/FREE policy. */
    void resetRuntime();

    /** @brief Copies the state a restart preparation depends on; caller holds the lock. */
    TempoSnapshot takeTempoSnapshotUnsafe() const;

    /** @brief Computes the first event offset and step phase of every GLOBAL channel. */
    void prepareRestart(const ScheduleInputs& inputs, PreparedRestart& restart) const;

    /** @brief Restarts master and GLOBAL schedules at the current position; no division. */
    void installRestartUnsafe(const PreparedRestart& restart);

    /**
     * @brief Applies @p request with the restart schedules prepared outside the lock (F-072).
     *
     * A preparation is current while the schedule generation is unchanged; otherwise
     * it repeats, and after kStagedSwapAttempts the request computes under the lock.
     */
    void applyTransportRequest(TransportRequest request);

    /**
     * @brief applyTransportRequest() that applies only if @p condition agrees under the lock.
     * @param request Transport change.
     * @param condition Called once under the installing lock; nullptr always applies.
     * @param context Passed to @p condition.
     * @return False when @p condition declined.
     */
    bool applyTransportRequestWhen(TransportRequest request, StartCondition condition, void* context);

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
     */
    void applyTransportRequestUnsafe(TransportRequest request, const PreparedRestart* restart);

    /** @brief True when @p request will restart the GLOBAL schedules (pre-check without lock). */
    bool transportRequestRestarts(TransportRequest request) const;

    /** @brief Counts one staged swap that computed under the lock (test and debug counters). */
    void countStagedSwapFallback();

    /** @brief Arms the silent STOP->PLAY count-in from the configured master-beat count. */
    void startPreCountUnsafe();

    /** @brief Finishes the count-in and restarts musical phase at the first audible boundary. */
    void finishPreCountUnsafe();

    /** @brief Drops the AUTO freewheel tempo after a manual tempo change while unlocked; caller holds the lock. */
    void discardFreewheelTempoAfterManualChangeUnsafe();

    /** @brief Copies master and channel configuration without entering a critical section. */
    void copyConfigurationUnsafe(const ClockState& state);

    /** @brief Points every groove, pattern and expression table at its own buffer. */
    void initializeBufferTables();

    /** @brief Resolves @p state into an engine configuration (no shared state). */
    static void resolveConfiguration(const ClockState& state, EngineConfiguration& configuration);

    /**
     * @brief Computes the Custom Groove spacing cache for every channel of one state (foreground).
     * @param state Application state that the following configuration swap installs.
     * @param spacingQ16 Receives one Q16 spacing per physical output.
     */
    void calculateCustomGrooveSpacingsQ16(
        const ClockState& state,
        std::uint32_t (&spacingQ16)[kChannelCount]) const;

    /** @brief Installs precomputed spacings; caller holds the configuration-swap critical section. */
    void installCustomGrooveSpacingsUnsafe(const std::uint32_t (&spacingQ16)[kChannelCount]);

    hal::GateOutputDriver& gateOutputs_;
    /**
     * Two configuration buffers (W-060). The scheduler reads *configuration_; the
     * foreground stages the other buffer outside the lock and installs it by
     * swapping this pointer under the lock.
     */
    EngineConfiguration configurationBuffers_[2]{};
    EngineConfiguration* configuration_ = &configurationBuffers_[0];
    volatile bool playing_ = true;
    volatile bool restartPending_ = true;
    volatile bool preCountActive_ = false;
    volatile std::uint8_t preCountRemaining_ = 0U;
    volatile std::uint64_t preCountPhaseQ32_ = 0U;
    volatile std::uint64_t preCountRemainder_ = 0U;
    volatile std::uint32_t preCountExternalPulseRemainder_ = 0U;
    volatile std::uint64_t masterPositionQ32_ = 0U;
    volatile std::uint64_t globalScheduleEpochQ32_ = 0U;
    volatile std::uint64_t masterBeatPhaseQ32_ = 0U;
    volatile std::uint32_t masterBeatSerial_ = 0U;
    volatile std::uint8_t masterBeat_ = 1U;
    volatile std::uint16_t masterBar_ = 1U;
    volatile bool externalLocked_ = false;
    volatile bool externalResetGate_ = false;
    volatile std::uint32_t externalBpmMilli_ = 0U;
    volatile bool externalPhaseInitialized_ = false;
    volatile std::uint64_t externalReferenceQ32_ = 0U;
    volatile std::uint64_t externalMusicalPositionQ32_ = 0U;
    volatile std::uint32_t externalPulseRemainder_ = 0U;
    ChannelRuntime channelRuntime_[kChannelCount]{};
    volatile std::uint32_t schedulerTickCounter_ = 0U;
    /**
     * Incremented whenever scheduler context moves a schedule basis other than by
     * processing an event: restart, phase correction, external tempo or lock.
     */
    volatile std::uint32_t scheduleGeneration_ = 0U;
    volatile std::uint64_t masterRemainder_ = 0U;
    /** TIM5 timestamp of the last counted TIM3 update (scheduler context only, F-012). */
    std::uint32_t lastSchedulerUpdateUs_ = 0U;
    /** False until the first scheduler interrupt anchored lastSchedulerUpdateUs_. */
    bool schedulerUpdateAnchored_ = false;
    bool schedulerQuantumOpen_ = false;
    std::uint8_t gateHighMask_ = 0U;
    std::uint8_t gatesHighAtLastCommit_ = 0U;
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
