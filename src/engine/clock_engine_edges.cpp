/**
 * @file clock_engine_edges.cpp
 * @brief Gate-edge batching per scheduler quantum and edge-preserving rescheduling for ClockEngine.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * All gate changes of one scheduler quantum are staged in the GateOutputDriver
 * and written with one GPIOB BSRR access at the end of processSchedulerTick()
 * (F-011). Because a channel then cannot fall and rise within one quantum, a
 * HIGH gate is ended one tick before its next rising edge, and only when that
 * is no longer possible does the rising edge move by one tick (F-015). A
 * reschedule never selects an event slot that has already been processed (F-063).
 */
#include "engine/clock_engine_edges.h"

#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
#include "hal/runtime_instrumentation.h"
#endif

namespace clockfw::engine {
namespace {

constexpr std::uint8_t channelBit(const std::size_t channelIndex) {
    return static_cast<std::uint8_t>(1U << channelIndex);
}

}  // namespace

void ClockEngine::beginSchedulerQuantumFromIsr() {
    schedulerQuantumOpen_ = true;
    gatesRaisedThisQuantum_ = 0U;
}

void ClockEngine::processSchedulerTick() {
    if (!schedulerQuantumOpen_) {
        beginSchedulerQuantumFromIsr();
    }
    runSchedulerTick();
    gateOutputs_.commitStagedChannels();
    gatesHighAtLastCommit_ = gateHighMask_;
    schedulerQuantumOpen_ = false;
}

void ClockEngine::setGateState(const std::size_t channelIndex, const bool high) {
    ChannelRuntime& runtime = channelRuntime_[channelIndex];
    const std::uint8_t bit = channelBit(channelIndex);
    if (!high) {
        runtime.gateOffScheduled = false;
        deferredRiseMask_ = static_cast<std::uint8_t>(deferredRiseMask_ & ~bit);
    }
    if (runtime.gateHigh == high) {
        return;
    }
    if (high && schedulerQuantumOpen_ && (gatesHighAtLastCommit_ & bit) != 0U) {
        // The gate was HIGH in the previous tick and falls in this one. Rising
        // in the same BSRR write would merge both pulses, so the rising edge
        // moves to the next tick (50 us).
        deferredRiseMask_ = static_cast<std::uint8_t>(deferredRiseMask_ | bit);
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
        hal::instrumentation::countDeferredGateRise();
#endif
#ifdef CLOCK_HOST_TEST
        ++deferredGateRiseCountForTest_;
#endif
        return;
    }
    runtime.gateHigh = high;
    gateHighMask_ = static_cast<std::uint8_t>(high ? (gateHighMask_ | bit) : (gateHighMask_ & ~bit));
    if (!schedulerQuantumOpen_) {
        // Foreground (always under InterruptLock) only ever lowers gates.
        gateOutputs_.setChannelState(channelIndex, high);
        return;
    }
    if (high) {
        gatesRaisedThisQuantum_ = static_cast<std::uint8_t>(gatesRaisedThisQuantum_ | bit);
    }
    gateOutputs_.stageChannelState(channelIndex, high);
}

void ClockEngine::serviceDeferredGateRises() {
    const std::uint8_t pending = deferredRiseMask_;
    if (pending == 0U) {
        return;
    }
    deferredRiseMask_ = 0U;
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        if ((pending & channelBit(channelIndex)) != 0U) {
            setGateState(channelIndex, true);
        }
    }
}

void ClockEngine::endGatesBeforeNextRise(const std::uint64_t masterIncrementQ32) {
    // Only HIGH gates that did not rise in this quantum need the look-ahead.
    const std::uint8_t candidates = static_cast<std::uint8_t>(gateHighMask_ & ~gatesRaisedThisQuantum_);
    if (candidates == 0U) {
        return;
    }
    // The next tick advances the master by this increment or by one unit more
    // (remainder carry). A gate whose next rising edge lies within that reach
    // ends now, so the output is LOW for at least the current tick.
    const std::uint64_t positionQ32 = masterPositionQ32_;
    const std::uint64_t reachQ32 = positionQ32 > UINT64_MAX - masterIncrementQ32 - 1U
        ? UINT64_MAX
        : positionQ32 + masterIncrementQ32 + 1U;
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        const ChannelRuntime& runtime = channelRuntime_[channelIndex];
        if ((candidates & channelBit(channelIndex)) == 0U || runtime.tieHold) {
            continue;
        }
        std::uint64_t nextRiseQ32 = runtime.nextEventQ32;
        if (runtime.ratchetRemaining != 0U && runtime.nextRatchetQ32 < nextRiseQ32) {
            nextRiseQ32 = runtime.nextRatchetQ32;
        }
        if (nextRiseQ32 <= reachQ32) {
            setGateState(channelIndex, false);
        }
    }
}

std::uint64_t ClockEngine::firstSerialAfterLastEvent(
    const std::size_t channelIndex,
    std::uint64_t eventSerial) const {
    const ChannelRuntime& runtime = channelRuntime_[channelIndex];
    if (!runtime.lastEventSlotValid) {
        return eventSerial;
    }
    const std::uint64_t epochQ32 = runtime.scheduleEpochQ32;
    const auto slotOf = [&](const std::uint64_t serial) {
        const std::uint64_t offsetQ32 = calculateBaseEventOffsetQ32(channelIndex, serial);
        return offsetQ32 > UINT64_MAX - epochQ32 ? UINT64_MAX : epochQ32 + offsetQ32;
    };
    const std::uint64_t lastSlotQ32 = runtime.lastEventSlotQ32;
    if (slotOf(eventSerial) > lastSlotQ32) {
        return eventSerial;
    }
    // Jump close to the first later slot, then correct exactly in both directions
    // without going below the first event that is still in the future.
    const std::uint64_t earliestSerial = eventSerial;
    const std::uint64_t intervalQ32 = calculateNominalBaseIntervalQ32(channelIndex);
    if (intervalQ32 != 0U && lastSlotQ32 > epochQ32) {
        const std::uint64_t estimate = (lastSlotQ32 - epochQ32) / intervalQ32;
        if (estimate > eventSerial) {
            eventSerial = estimate;
        }
    }
    std::uint8_t guard = 0U;
    while (eventSerial > earliestSerial && slotOf(eventSerial - 1U) > lastSlotQ32 && guard++ < 4U) {
        --eventSerial;
    }
    guard = 0U;
    while (slotOf(eventSerial) <= lastSlotQ32 && eventSerial != UINT64_MAX && guard++ < 4U) {
        ++eventSerial;
    }
    return eventSerial;
}

#ifdef CLOCK_HOST_TEST
std::uint32_t ClockEngine::deferredGateRiseCountForTest() const {
    return deferredGateRiseCountForTest_;
}
#endif

}  // namespace clockfw::engine
