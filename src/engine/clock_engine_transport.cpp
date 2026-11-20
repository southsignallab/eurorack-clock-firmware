/**
 * @file clock_engine_transport.cpp
 * @brief Play, stop and reset with restart schedules prepared outside the interrupt lock.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * A restart puts the GLOBAL epoch on the current master position P. Every term of
 * an event position is relative to the epoch, so the first event of a GLOBAL
 * channel is P plus an offset that depends only on the configuration and the
 * tempo. The foreground computes these offsets and step phases outside the lock;
 * the lock adds P and installs them (F-072). The preparation is current while the
 * schedule generation is unchanged (external tempo, lock, restart, phase
 * correction); otherwise it repeats, and after kStagedSwapAttempts the request
 * computes under the lock as before. The scheduler interrupt prepares and installs
 * in one step (resetRuntime()).
 */
#include "engine/clock_engine_transport.h"

#include "clock_core.h"
#include "hal/interrupt_lock.h"

#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
#include "hal/runtime_instrumentation.h"
#endif

namespace clockfw::engine {

void ClockEngine::play() {
    applyTransportRequest(TransportRequest::Play);
}

void ClockEngine::playFromIsr() {
    applyTransportRequestUnsafe(TransportRequest::Play, nullptr);
}

void ClockEngine::stopFromIsr() {
    applyTransportRequestUnsafe(TransportRequest::Stop, nullptr);
}

bool ClockEngine::playWhen(const StartCondition condition, void* const context) {
    return applyTransportRequestWhen(TransportRequest::Play, condition, context);
}

bool ClockEngine::transportRequestRestarts(const TransportRequest request) const {
    if (request != TransportRequest::Play) {
        return true;
    }
    // Only a fresh start without Pre-Count restarts at once; resuming from PAUSE
    // and an armed Pre-Count prepare nothing. Read without the lock: in the
    // scheduler interrupt nothing changes meanwhile, and in the foreground a
    // wrong guess only costs a preparation or computes under the lock.
    return !playing_ && restartPending_ && configuration_->preCountSteps == 0U;
}

void ClockEngine::applyTransportRequest(const TransportRequest request) {
    (void)applyTransportRequestWhen(request, nullptr, nullptr);  // no condition: always applies
}

bool ClockEngine::applyTransportRequestWhen(
    const TransportRequest request,
    const StartCondition condition,
    void* const context) {
    if (transportRequestRestarts(request)) {
        // Not value-initialized: prepareRestart() writes every field (see PreparedRestart).
        PreparedRestart restart;  // NOLINT(cppcoreguidelines-pro-type-member-init)
        for (std::uint8_t attempt = 0U; attempt < kStagedSwapAttempts; ++attempt) {
            TempoSnapshot tempo{};
            {
                hal::InterruptLock interruptLock;
                tempo = takeTempoSnapshotUnsafe();
            }
            prepareRestart(foregroundScheduleInputs(tempo), restart);
#ifdef CLOCK_HOST_TEST
            if (configurationSwapHookForTest_ != nullptr) {
                configurationSwapHookForTest_(configurationSwapHookContextForTest_);
            }
#endif
            hal::InterruptLock interruptLock;
            if (scheduleGeneration_ == tempo.generation) {
                return applyTransportRequestIfUnsafe(request, &restart, condition, context);
            }
        }
        countStagedSwapFallback();
    }
    hal::InterruptLock interruptLock;
    return applyTransportRequestIfUnsafe(request, nullptr, condition, context);
}

bool ClockEngine::applyTransportRequestIfUnsafe(
    const TransportRequest request,
    const PreparedRestart* const restart,
    const StartCondition condition,
    void* const context) {
    if (condition != nullptr && !condition(context)) {
        return false;
    }
    applyTransportRequestUnsafe(request, restart);
    return true;
}

void ClockEngine::applyTransportRequestUnsafe(
    const TransportRequest request,
    const PreparedRestart* const restart) {
    if (request == TransportRequest::Play) {
        if (playing_) {
            return;
        }
        const bool freshStart = restartPending_;
        playing_ = true;
        if (!freshStart) {
            return;
        }
        startPreCountUnsafe();
        if (preCountActive_) {
            return;
        }
        // Without Pre-Count a fresh start begins at step 1 and beat 1 now;
        // external pulses received while stopped must not shift that downbeat.
    } else if (request == TransportRequest::Stop) {
        playing_ = false;
        preCountActive_ = false;
        preCountRemaining_ = 0U;
        preCountPhaseQ32_ = 0U;
        preCountRemainder_ = 0U;
        preCountExternalPulseRemainder_ = 0U;
        // The restart below lowers every gate.
        for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
            ChannelRuntime& runtime = channelRuntime_[channelIndex];
            runtime.tieHold = false;
            runtime.ratchetRemaining = 0U;
        }
    }
    if (restart != nullptr) {
        installRestartUnsafe(*restart);
    } else {
        resetRuntime();
    }
    if (request == TransportRequest::Play && externalPhaseInitialized_) {
        externalReferenceQ32_ = masterPositionQ32_;
        externalMusicalPositionQ32_ = 0U;
        externalPulseRemainder_ = 0U;
    }
}

void ClockEngine::stop() {
    applyTransportRequest(TransportRequest::Stop);
}

void ClockEngine::resetGlobalPhase() {
    applyTransportRequest(TransportRequest::Reset);
}

void ClockEngine::resetGlobalPhaseFromIsr() {
    resetRuntime();
}

void ClockEngine::resetRuntime() {
    // Scheduler context or under the lock: prepare and install at once. Not value-initialized:
    // prepareRestart() writes every field.
    PreparedRestart restart;  // NOLINT(cppcoreguidelines-pro-type-member-init)
    prepareRestart(liveScheduleInputs(), restart);
    installRestartUnsafe(restart);
}

ClockEngine::TempoSnapshot ClockEngine::takeTempoSnapshotUnsafe() const {
    TempoSnapshot tempo{};
    tempo.generation = scheduleGeneration_;
    tempo.externalLocked = externalLocked_;
    tempo.externalBpmMilli = externalBpmMilli_;
    return tempo;
}

void ClockEngine::prepareRestart(const ScheduleInputs& inputs, PreparedRestart& restart) const {
#ifdef CLOCK_HOST_TEST
    countCalculationUnderLockForTest();
#endif
    restart.resetMask = 0U;
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        if (!core::shouldResetChannel(
                inputs.configuration->channels[channelIndex].common.resetMode, false)) {
            restart.firstEventOffsetQ32[channelIndex] = 0U;
            restart.phases[channelIndex] = PreparedStepPhase{};
            continue;
        }
        restart.resetMask = static_cast<std::uint8_t>(restart.resetMask | (1U << channelIndex));
        // Epoch 0 and serial 0: the restart puts the epoch on the master position,
        // which every term of the event position is relative to. A GLOBAL channel
        // derives its step from the serial, so the phase holds no FREE remainder.
        restart.firstEventOffsetQ32[channelIndex] =
            calculateEventPositionQ32(inputs, channelIndex, 0U, 0U);
        restart.phases[channelIndex] = prepareStepPhase(
            *inputs.configuration, channelIndex, 0U, liveSequencerPattern(channelIndex));
    }
}

void ClockEngine::installRestartUnsafe(const PreparedRestart& restart) {
#ifdef CLOCK_HOST_TEST
    ++resetRuntimeCountForTest_;
#endif
    restartPending_ = true;
    ++scheduleGeneration_;
    const std::uint64_t positionQ32 = masterPositionQ32_;
    globalScheduleEpochQ32_ = positionQ32;
    masterBeatPhaseQ32_ = 0U;
    masterBeatSerial_ = 0U;
    masterRemainder_ = 0U;
    masterBeat_ = 1U;
    masterBar_ = 1U;
    // The bar/beat display follows external pulses from this accumulator; it restarts with
    // the counter, or the next pulse would show the position before the reset (F-027).
    externalMusicalPositionQ32_ = 0U;

    lowerAllGatesUnsafe();
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        if (((static_cast<std::uint32_t>(restart.resetMask) >> channelIndex) & 1U) == 0U) {
            continue;
        }

        ChannelRuntime& runtime = channelRuntime_[channelIndex];
        runtime.step = restart.phases[channelIndex].step;
        runtime.displayedStep = restart.phases[channelIndex].displayedStep;
        runtime.gateOffTick = 0U;
        runtime.gateOffScheduled = false;
        runtime.tieHold = false;
        runtime.ratchetRemaining = 0U;
        runtime.nextRatchetQ32 = 0U;
        runtime.ratchetSpacingQ32 = 0U;
        runtime.ratchetGateTicks = 1U;
        runtime.lastEventSlotValid = false;
        // Serial 0 on the new epoch; its grid slot is the epoch itself.
        const std::uint64_t offsetQ32 = restart.firstEventOffsetQ32[channelIndex];
        runtime.scheduleEpochQ32 = positionQ32;
        runtime.nextEventSerial = 0U;
        runtime.nextEventQ32 =
            offsetQ32 > UINT64_MAX - positionQ32 ? UINT64_MAX : positionQ32 + offsetQ32;
        runtime.nextEventSlotQ32 = positionQ32;
    }
}

void ClockEngine::countStagedSwapFallback() {
#ifdef CLOCK_HOST_TEST
    ++configurationSwapFallbackCountForTest_;
#endif
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
    hal::instrumentation::countStagedSwapFallback();
#endif
}

}  // namespace clockfw::engine
