/**
 * @file clock_engine_schedule.cpp
 * @brief Channel rescheduling and configuration swaps with a short interrupt lock.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * A configuration change recomputes the next event of up to eight channels. Each
 * computation needs several 64-bit divisions. They run in the foreground against a
 * staged second configuration buffer and a snapshot of the scheduler state; the
 * interrupt lock covers only the swap: one pointer store for the configuration and
 * the prepared per-channel values, no division and no byte-wise copy (F-012, F-068).
 *
 * The scheduler interrupt keeps running during the preparation. Advancing the
 * master position alone keeps a prepared schedule valid: an event that the
 * preparation placed between the snapshot and the swap fires in the next tick.
 * Everything else that moves a schedule basis invalidates it: a processed event of
 * that channel (its next serial changes) or a restart, phase correction, external
 * tempo or lock change (scheduleGeneration_). The swap then repeats the
 * preparation, at most kStagedSwapAttempts times, and finally computes under the
 * lock as before, so a change is never lost.
 *
 * Custom Groove library and preview changes and Sequencer 2.0 pattern changes use
 * the same swap (F-072): the new pattern goes into a spare buffer and a copy of
 * the groove table outside the lock, and the lock exchanges pointers.
 */
#include "engine/clock_engine_schedule.h"

#include "hal/interrupt_lock.h"

namespace clockfw::engine {
namespace {

constexpr std::uint8_t kAllChannelsMask = static_cast<std::uint8_t>((1U << kChannelCount) - 1U);
static_assert(kChannelCount <= 8U, "Channel masks are 8 bits wide");

constexpr bool channelInMask(const std::uint8_t mask, const std::size_t channelIndex) {
    return ((static_cast<std::uint32_t>(mask) >> channelIndex) & 1U) != 0U;
}

}  // namespace

ClockEngine::EngineConfiguration* ClockEngine::inactiveConfiguration() {
    // Foreground only: the scheduler never reads the buffer that is not installed.
    return configuration_ == &configurationBuffers_[0] ? &configurationBuffers_[1]
                                                       : &configurationBuffers_[0];
}

ClockEngine::GrooveSources* ClockEngine::stageGrooveSources() {
    // Foreground only, like the configuration buffers; no pattern is copied here.
    GrooveSources* const staged = grooveSources_ == &grooveSourceBuffers_[0]
        ? &grooveSourceBuffers_[1]
        : &grooveSourceBuffers_[0];
    *staged = *grooveSources_;
    return staged;
}

ClockEngine::StagedConfiguration ClockEngine::stageLiveConfiguration(
    GrooveSources* const grooves) const {
    StagedConfiguration staged{};
    // The installed configuration stays: the foreground is its only writer.
    staged.configuration = configuration_;
    staged.grooves = grooves;
    for (std::size_t index = 0U; index < kChannelCount; ++index) {
        staged.customGrooveSpacingQ16[index] = customGrooveSpacingQ16_[index];
    }
    return staged;
}

ClockEngine::ScheduleInputs ClockEngine::foregroundScheduleInputs(
    const TempoSnapshot& tempo) const {
    return makeScheduleInputs(*configuration_, customGrooveSpacingQ16_, *grooveSources_,
                              tempo.externalLocked, tempo.externalBpmMilli);
}

const SequencerPatternV2* ClockEngine::liveSequencerPattern(const std::size_t channelIndex) const {
    return sequencerPatternV2Active_[channelIndex] ? sequencerPatterns_[channelIndex] : nullptr;
}

void ClockEngine::updateConfiguration(const ClockState& state, const bool rescheduleChannels) {
    StagedConfiguration staged{};
    staged.configuration = inactiveConfiguration();
    staged.grooves = grooveSources_;
    resolveConfiguration(state, *staged.configuration);
    calculateCustomGrooveSpacingsQ16(state, staged.customGrooveSpacingQ16);
    swapConfiguration(staged, kAllChannelsMask, rescheduleChannels);
}

void ClockEngine::updateChannel(
    const std::size_t channelIndex,
    const ChannelConfig& channelConfiguration,
    const bool rescheduleChannel) {
    if (channelIndex >= kChannelCount) {
        return;
    }
    // The foreground is the only writer of the configuration and the spacings.
    StagedConfiguration staged{};
    staged.configuration = inactiveConfiguration();
    staged.grooves = grooveSources_;
    *staged.configuration = *configuration_;
    for (std::size_t index = 0U; index < kChannelCount; ++index) {
        staged.customGrooveSpacingQ16[index] = customGrooveSpacingQ16_[index];
    }
    staged.configuration->channels[channelIndex] = channelConfiguration;
    staged.customGrooveSpacingQ16[channelIndex] =
        calculateCustomGrooveSpacingQ16(*grooveSources_, channelIndex, channelConfiguration);
    swapConfiguration(
        staged, static_cast<std::uint8_t>(1U << channelIndex), rescheduleChannel);
}

void ClockEngine::swapConfiguration(
    const StagedConfiguration& staged,
    const std::uint8_t channelMask,
    const bool rescheduleChannels) {
    PreparedChannelSchedule schedules[kChannelCount]{};
    PreparedStepPhase phases[kChannelCount]{};
    ScheduleSnapshot snapshot{};
    for (std::uint8_t attempt = 0U; attempt < kStagedSwapAttempts; ++attempt) {
        {
            hal::InterruptLock interruptLock;
            takeScheduleSnapshotUnsafe(snapshot);
        }
        prepareConfigurationSwap(
            staged, snapshot, channelMask, rescheduleChannels, schedules, phases);
#ifdef CLOCK_HOST_TEST
        if (configurationSwapHookForTest_ != nullptr) {
            configurationSwapHookForTest_(configurationSwapHookContextForTest_);
        }
#endif
        hal::InterruptLock interruptLock;
        if (scheduleSnapshotCurrentUnsafe(snapshot, channelMask)) {
            installConfigurationSwapUnsafe(
                staged, channelMask, rescheduleChannels, schedules, phases);
            return;
        }
    }
    // The scheduler kept moving the basis: compute under the lock, as before W-060.
    countStagedSwapFallback();
    hal::InterruptLock interruptLock;
    takeScheduleSnapshotUnsafe(snapshot);
    prepareConfigurationSwap(staged, snapshot, channelMask, rescheduleChannels, schedules, phases);
    installConfigurationSwapUnsafe(staged, channelMask, rescheduleChannels, schedules, phases);
}

void ClockEngine::takeScheduleSnapshotUnsafe(ScheduleSnapshot& snapshot) const {
    snapshot.generation = scheduleGeneration_;
    snapshot.positionQ32 = masterPositionQ32_;
    snapshot.globalEpochQ32 = globalScheduleEpochQ32_;
    snapshot.restartPending = restartPending_;
    snapshot.externalLocked = externalLocked_;
    snapshot.externalBpmMilli = externalBpmMilli_;
    snapshot.configuredBpm = configuration_->bpm;
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        const ChannelRuntime& runtime = channelRuntime_[channelIndex];
        snapshot.epochQ32[channelIndex] = runtime.scheduleEpochQ32;
        snapshot.nextEventSerial[channelIndex] = runtime.nextEventSerial;
        snapshot.lastEventSlotQ32[channelIndex] = runtime.lastEventSlotQ32;
        snapshot.lastEventSlotValid[channelIndex] = runtime.lastEventSlotValid;
    }
}

void ClockEngine::prepareConfigurationSwap(
    const StagedConfiguration& staged,
    const ScheduleSnapshot& snapshot,
    const std::uint8_t channelMask,
    const bool rescheduleChannels,
    PreparedChannelSchedule (&schedules)[kChannelCount],
    PreparedStepPhase (&phases)[kChannelCount]) const {
    // A manual tempo change drops the AUTO freewheel tempo at the swap (D-02);
    // the preparation uses the tempo that will be in effect after it.
    const EngineConfiguration& configuration = *staged.configuration;
    const bool freewheelTempoDropped = configuration.bpm != snapshot.configuredBpm &&
        configuration.source == ClockSource::Auto && !snapshot.externalLocked;
    const ScheduleInputs inputs = makeScheduleInputs(
        configuration,
        staged.customGrooveSpacingQ16,
        *staged.grooves,
        snapshot.externalLocked,
        freewheelTempoDropped ? 0U : snapshot.externalBpmMilli);
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        if (!channelInMask(channelMask, channelIndex)) {
            continue;
        }
        std::uint64_t nextEventSerial = snapshot.nextEventSerial[channelIndex];
        if (rescheduleChannels) {
            ChannelScheduleBasis basis{};
            basis.positionQ32 = snapshot.positionQ32;
            const bool global =
                configuration.channels[channelIndex].common.resetMode == ResetMode::Global;
            basis.epochQ32 = global ? snapshot.globalEpochQ32 : snapshot.epochQ32[channelIndex];
            basis.lastEventSlotQ32 = snapshot.lastEventSlotQ32[channelIndex];
            basis.lastEventSlotValid = snapshot.lastEventSlotValid[channelIndex];
            basis.mayUseCurrentBoundary = snapshot.restartPending;
            schedules[channelIndex] = prepareChannelSchedule(inputs, channelIndex, basis);
            nextEventSerial = schedules[channelIndex].nextEventSerial;
        }
        const SequencerPatternV2* const pattern = channelIndex == staged.sequencerChannel
            ? staged.sequencerPattern
            : liveSequencerPattern(channelIndex);
        phases[channelIndex] =
            prepareStepPhase(configuration, channelIndex, nextEventSerial, pattern);
    }
}

bool ClockEngine::scheduleSnapshotCurrentUnsafe(
    const ScheduleSnapshot& snapshot,
    const std::uint8_t channelMask) const {
    if (scheduleGeneration_ != snapshot.generation) {
        return false;
    }
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        const std::uint64_t liveSerial = channelRuntime_[channelIndex].nextEventSerial;
        if (channelInMask(channelMask, channelIndex) &&
            liveSerial != snapshot.nextEventSerial[channelIndex]) {
            return false;
        }
    }
    return true;
}

void ClockEngine::installConfigurationSwapUnsafe(
    const StagedConfiguration& staged,
    const std::uint8_t channelMask,
    const bool rescheduleChannels,
    const PreparedChannelSchedule (&schedules)[kChannelCount],
    const PreparedStepPhase (&phases)[kChannelCount]) {
    const std::uint16_t previousBpm = configuration_->bpm;
    // The staged buffers become the scheduler's configuration and groove table:
    // pointer stores only, no copy of a configuration or a pattern (F-071).
    configuration_ = staged.configuration;
    grooveSources_ = staged.grooves;
    if (staged.releasedGroove != nullptr) {
        spareCustomGroove_ = staged.releasedGroove;
    }
    installCustomGrooveSpacingsUnsafe(staged.customGrooveSpacingQ16);
    if (staged.sequencerChannel < kChannelCount) {
        installSequencerPatternUnsafe(staged.sequencerChannel, staged.sequencerPattern);
    }
    if (configuration_->bpm != previousBpm) {
        discardFreewheelTempoAfterManualChangeUnsafe();
    }
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        if (!channelInMask(channelMask, channelIndex)) {
            continue;
        }
        const ChannelConfig& channel = configuration_->channels[channelIndex];
        if (channel.common.mode == ChannelMode::Off || channel.common.muted) {
            setGateState(channelIndex, false);
        }
        if (rescheduleChannels) {
            installChannelSchedule(channelIndex, schedules[channelIndex]);
        }
        installStepPhase(channelIndex, phases[channelIndex]);
    }
}

void ClockEngine::scheduleChannelFromCurrentPosition(
    const std::size_t channelIndex,
    const bool includeCurrentBoundary) {
    installChannelSchedule(
        channelIndex,
        prepareChannelSchedule(
            liveScheduleInputs(),
            channelIndex,
            liveScheduleBasis(channelIndex, includeCurrentBoundary)));

    // GLOBAL means pattern position as well as edge timing is derived from the
    // shared epoch. Otherwise a live CLOCK -> EUCLID/SEQ switch can be on-grid
    // in time while evaluating the wrong pattern step.
    if (configuration_->channels[channelIndex].common.resetMode == ResetMode::Global) {
        synchronizeChannelStepPhase(channelIndex);
    }
}

ClockEngine::ChannelScheduleBasis ClockEngine::liveScheduleBasis(
    const std::size_t channelIndex,
    const bool includeCurrentBoundary) const {
    const ChannelRuntime& runtime = channelRuntime_[channelIndex];
    ChannelScheduleBasis basis{};
    basis.positionQ32 = masterPositionQ32_;
    basis.epochQ32 = configuration_->channels[channelIndex].common.resetMode == ResetMode::Global
        ? globalScheduleEpochQ32_
        : runtime.scheduleEpochQ32;
    basis.lastEventSlotQ32 = runtime.lastEventSlotQ32;
    basis.lastEventSlotValid = runtime.lastEventSlotValid;
    basis.mayUseCurrentBoundary = includeCurrentBoundary || restartPending_;
    return basis;
}

#ifdef CLOCK_HOST_TEST
void ClockEngine::countCalculationUnderLockForTest() const {
    if (hal::InterruptLock::activeCountForTest() != 0U) {
        ++scheduleCalculationsUnderLockForTest_;
    }
}
#endif

ClockEngine::PreparedChannelSchedule ClockEngine::prepareChannelSchedule(
    const ScheduleInputs& inputs,
    const std::size_t channelIndex,
    const ChannelScheduleBasis& basis) const {
#ifdef CLOCK_HOST_TEST
    countCalculationUnderLockForTest();
#endif
    const std::uint64_t epochQ32 = basis.epochQ32;
    const std::uint64_t positionQ32 = basis.positionQ32;
    const bool mayUseCurrentBoundary = basis.mayUseCurrentBoundary;
    const std::uint64_t firstEventQ32 =
        calculateEventPositionQ32(inputs, channelIndex, epochQ32, 0U);
    std::uint64_t eventSerial = 0U;

    if (positionQ32 > firstEventQ32 ||
        (positionQ32 == firstEventQ32 && !mayUseCurrentBoundary)) {
        const std::uint64_t elapsedQ32 = positionQ32 - firstEventQ32;
        const std::uint64_t baseIntervalQ32 = calculateNominalBaseIntervalQ32(inputs, channelIndex);
        eventSerial = baseIntervalQ32 != 0U ? elapsedQ32 / baseIntervalQ32 : 0U;

        // The estimate above is intentionally cheap. Rational grids use exact
        // cumulative offsets below, so correct both directions before selecting
        // the first event that is still eligible.
        while (eventSerial > 0U) {
            const std::uint64_t previousPositionQ32 = calculateEventPositionQ32(
                inputs, channelIndex, epochQ32, eventSerial - 1U);
            const bool previousEligible = mayUseCurrentBoundary
                ? previousPositionQ32 >= positionQ32
                : previousPositionQ32 > positionQ32;
            if (!previousEligible) {
                break;
            }
            --eventSerial;
        }

        for (;;) {
            const std::uint64_t eventPositionQ32 =
                calculateEventPositionQ32(inputs, channelIndex, epochQ32, eventSerial);
            const bool eligible = mayUseCurrentBoundary
                ? eventPositionQ32 >= positionQ32
                : eventPositionQ32 > positionQ32;
            if (eligible || eventSerial == UINT64_MAX) {
                break;
            }
            ++eventSerial;
        }
    }

    // A reschedule never re-emits an event slot that has already been processed.
    eventSerial = firstSerialAfterLastEvent(inputs, channelIndex, basis, eventSerial);
    PreparedChannelSchedule schedule;
    schedule.epochQ32 = epochQ32;
    schedule.nextEventSerial = eventSerial;
    schedule.nextEventQ32 = calculateEventPositionQ32(
        inputs, channelIndex, epochQ32, eventSerial, &schedule.nextEventSlotQ32);
    return schedule;
}

void ClockEngine::installChannelSchedule(
    const std::size_t channelIndex,
    const PreparedChannelSchedule& schedule) {
    ChannelRuntime& runtime = channelRuntime_[channelIndex];
    runtime.scheduleEpochQ32 = schedule.epochQ32;
    runtime.nextEventSerial = schedule.nextEventSerial;
    runtime.nextEventQ32 = schedule.nextEventQ32;
    runtime.nextEventSlotQ32 = schedule.nextEventSlotQ32;
}

std::uint8_t ClockEngine::channelCycleLength(
    const EngineConfiguration& configuration,
    const std::size_t channelIndex) {
    const ChannelConfig& channel = configuration.channels[channelIndex];
    if (channel.common.mode == ChannelMode::Euclid) {
        return channel.euclid.steps != 0U ? channel.euclid.steps : 1U;
    }
    if (channel.common.mode == ChannelMode::Sequencer) {
        return channel.sequencer.length != 0U ? channel.sequencer.length : 1U;
    }
    if (channel.common.mode == ChannelMode::Clock) {
        return channel.clock.meter.beats != 0U ? channel.clock.meter.beats : 1U;
    }
    return 1U;
}

void ClockEngine::synchronizeChannelStepPhase(const std::size_t channelIndex) {
    const std::uint64_t nextEventSerial = channelRuntime_[channelIndex].nextEventSerial;
    installStepPhase(
        channelIndex,
        prepareStepPhase(
            *configuration_, channelIndex, nextEventSerial, liveSequencerPattern(channelIndex)));
}

ClockEngine::PreparedStepPhase ClockEngine::prepareStepPhase(
    const EngineConfiguration& configuration,
    const std::size_t channelIndex,
    const std::uint64_t nextEventSerial,
    const SequencerPatternV2* const sequencerPattern) const {
#ifdef CLOCK_HOST_TEST
    countCalculationUnderLockForTest();
#endif
    PreparedStepPhase phase{};
    if (configuration.channels[channelIndex].common.mode == ChannelMode::Sequencer &&
        sequencerPattern != nullptr) {
        const SequencerPatternV2& pattern = *sequencerPattern;
        const std::uint32_t seed = 0x53455132UL ^
            (static_cast<std::uint32_t>(channelIndex + 1U) * 0x9E3779B9UL);
        const SequencerTraversalResult current = resolveSequencerTraversal(
            nextEventSerial, pattern, seed);
        const SequencerTraversalResult previous = resolveSequencerTraversal(
            nextEventSerial == 0U ? 0U : nextEventSerial - 1U,
            pattern,
            seed);
        phase.derivedFromSerial = true;
        phase.step = current.active ? rotateSequencerStep(current.step, pattern) : 0U;
        phase.displayedStep = previous.active ? rotateSequencerStep(previous.step, pattern) : 0U;
        return phase;
    }

    if (configuration.channels[channelIndex].common.resetMode == ResetMode::Global) {
        const std::uint8_t cycleLength = channelCycleLength(configuration, channelIndex);
        phase.derivedFromSerial = true;
        phase.step = static_cast<std::uint8_t>(nextEventSerial % cycleLength);
        phase.displayedStep = nextEventSerial == 0U
            ? static_cast<std::uint8_t>(0U)
            : static_cast<std::uint8_t>((nextEventSerial - 1ULL) % cycleLength);
    }
    return phase;
}

void ClockEngine::installStepPhase(const std::size_t channelIndex, const PreparedStepPhase& phase) {
    ChannelRuntime& runtime = channelRuntime_[channelIndex];
    if (phase.derivedFromSerial) {
        runtime.step = phase.step;
        runtime.displayedStep = phase.displayedStep;
        return;
    }
    // FREE keeps its local phase; only bound it (8-bit, no 64-bit division).
    const std::uint8_t cycleLength = channelCycleLength(*configuration_, channelIndex);
    runtime.step = static_cast<std::uint8_t>(runtime.step % cycleLength);
    runtime.displayedStep = static_cast<std::uint8_t>(runtime.displayedStep % cycleLength);
}

}  // namespace clockfw::engine
