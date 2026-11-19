/**
 * @file clock_engine_capture.cpp
 * @brief Read-only timing helpers used by high-resolution Groove Record capture.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "engine/clock_engine.h"

#include "clock_core.h"
#include "config.h"
#include "hal/interrupt_lock.h"

namespace clockfw::engine {

std::uint64_t ClockEngine::nominalIntervalQ32(const std::size_t channelIndex) const {
    if (channelIndex >= kChannelCount) {
        return 0U;
    }
    // Foreground: the configuration has no other writer; only the external tempo
    // is copied under the lock, the division runs after it (F-072).
    TempoSnapshot tempo{};
    {
        hal::InterruptLock interruptLock;
        tempo = takeTempoSnapshotUnsafe();
    }
#ifdef CLOCK_HOST_TEST
    countCalculationUnderLockForTest();
#endif
    return calculateNominalBaseIntervalQ32(foregroundScheduleInputs(tempo), channelIndex);
}

std::uint64_t ClockEngine::channelGridOriginQ32(const std::size_t channelIndex) const {
    if (channelIndex >= kChannelCount) {
        return 0U;
    }
    TempoSnapshot tempo{};
    std::uint64_t epochQ32 = 0U;
    {
        hal::InterruptLock interruptLock;
        tempo = takeTempoSnapshotUnsafe();
        epochQ32 = channelRuntime_[channelIndex].scheduleEpochQ32;
    }
#ifdef CLOCK_HOST_TEST
    countCalculationUnderLockForTest();
#endif
    const ScheduleInputs inputs = foregroundScheduleInputs(tempo);
    const std::uint64_t phaseOffsetQ32 = core::calculatePhaseOffsetQ32(
        calculateNominalBaseIntervalQ32(inputs, channelIndex),
        inputs.configuration->channels[channelIndex].common.phasePercent);
    return phaseOffsetQ32 > UINT64_MAX - epochQ32 ? UINT64_MAX : epochQ32 + phaseOffsetQ32;
}

std::uint64_t ClockEngine::estimateMasterPositionAtUs(
    const std::uint32_t eventTimestampUs,
    const std::uint32_t nowUs) const {
    TempoSnapshot tempo{};
    std::uint64_t positionQ32 = 0U;
    {
        hal::InterruptLock interruptLock;
        tempo = takeTempoSnapshotUnsafe();
        positionQ32 = masterPositionQ32_;
    }
#ifdef CLOCK_HOST_TEST
    countCalculationUnderLockForTest();
#endif
    const std::uint32_t elapsedUs = nowUs - eventTimestampUs;
    // A debounced front-panel edge should be only about 25 ms old. Bounding the
    // compensation avoids treating an invalid/stale timestamp as a wrap-sized
    // rewind while still covering unusually slow foreground service.
    const std::uint32_t boundedElapsedUs = elapsedUs > 100'000U ? 100'000U : elapsedUs;
    const std::uint64_t elapsedTicks = boundedElapsedUs / config::kSchedulerTickUs;
    volatile std::uint64_t remainder = 0U;
    const std::uint64_t incrementQ32 = core::calculateMasterIncrementMilliBpmQ32(
        effectiveBpmMilli(*configuration_, tempo.externalLocked, tempo.externalBpmMilli),
        configuration_->beatUnit,
        config::kSchedulerFrequencyHz,
        remainder);
    const std::uint64_t rewindQ32 = incrementQ32 != 0U &&
            elapsedTicks > UINT64_MAX / incrementQ32
        ? UINT64_MAX
        : incrementQ32 * elapsedTicks;
    return rewindQ32 >= positionQ32 ? 0U : positionQ32 - rewindQ32;
}

}  // namespace clockfw::engine
