/**
 * @file clock_engine_sync.cpp
 * @brief External synchronization and phase-alignment logic for ClockEngine.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "engine/clock_engine.h"

#include <cstdint>

#include "clock_core.h"
#include "hal/interrupt_lock.h"

namespace clockfw::engine {

void ClockEngine::setExternalLock(const bool locked, const std::uint32_t bpmMilli) {
    hal::InterruptLock interruptLock;
    setExternalLockFromIsr(locked, bpmMilli);
}

void ClockEngine::setExternalLockFromIsr(const bool locked, const std::uint32_t bpmMilli) {
    if (externalLocked_ != locked || externalBpmMilli_ != bpmMilli) {
        // The effective tempo and the shortest interval may change (W-060 swap check).
        ++scheduleGeneration_;
    }
    externalLocked_ = locked;
    externalBpmMilli_ = bpmMilli;
    if (!locked) {
        externalPhaseInitialized_ = false;
        externalPulseRemainder_ = 0U;
        externalMusicalPositionQ32_ = 0U;
    }
}

void ClockEngine::setExternalResetGateFromIsr(const bool high) {
    if (externalResetGate_ == high) {
        return;
    }
    externalResetGate_ = high;
    resetRuntime();
}

void ClockEngine::acceptExternalPulse(
    const std::uint32_t bpmMilli,
    const std::uint8_t pulsesPerQuarterNote) {
    hal::InterruptLock interruptLock;
    acceptExternalPulseUnsafe(bpmMilli, pulsesPerQuarterNote);
}

void ClockEngine::acceptExternalPulseFromIsr(
    const std::uint32_t bpmMilli,
    const std::uint8_t pulsesPerQuarterNote) {
    acceptExternalPulseUnsafe(bpmMilli, pulsesPerQuarterNote);
}

void ClockEngine::acceptExternalPulseUnsafe(
    const std::uint32_t bpmMilli,
    std::uint8_t pulsesPerQuarterNote) {
    if (!externalLocked_ || externalBpmMilli_ != bpmMilli) {
        ++scheduleGeneration_;
    }
    externalLocked_ = true;
    externalBpmMilli_ = bpmMilli;
    if (configuration_->source == ClockSource::Internal) {
        return;
    }
    if (pulsesPerQuarterNote == 0U) {
        pulsesPerQuarterNote = 1U;
    }

    if (preCountActive_) {
        // Convert every accepted external pulse into the same master-beat Q32
        // domain used by internal timing. A quarter note therefore advances one
        // beat in 4/4, two beats in */8, four in */16 and half a beat in */2.
        // This preserves PPQN precision without hard-coding quarter-note count-in.
        const std::uint32_t pulseNumerator =
            static_cast<std::uint32_t>(4U) * pulsesPerQuarterNote;
        const std::uint32_t pulseDenominator =
            configuration_->beatUnit != 0U ? configuration_->beatUnit : 4U;
        const std::uint64_t pulseIntervalQ32 = core::calculateNextIntervalQ32(
            pulseNumerator,
            pulseDenominator,
            preCountExternalPulseRemainder_);
        preCountPhaseQ32_ += pulseIntervalQ32;
        preCountRemainder_ = 0U;

        while (preCountPhaseQ32_ >= core::kQ32One && preCountRemaining_ > 0U) {
            preCountPhaseQ32_ -= core::kQ32One;
            --preCountRemaining_;
        }

        if (preCountRemaining_ == 0U) {
            finishPreCountUnsafe();
            externalReferenceQ32_ = masterPositionQ32_;
            externalMusicalPositionQ32_ = 0U;
            externalPulseRemainder_ = 0U;
            externalPhaseInitialized_ = true;
        }
        return;
    }

    if (!externalPhaseInitialized_) {
        externalReferenceQ32_ = masterPositionQ32_;
        externalMusicalPositionQ32_ = 0U;
        externalPulseRemainder_ = 0U;
        resetRuntime();
        externalPhaseInitialized_ = true;
        return;
    }

    const std::uint32_t pulseNumerator =
        static_cast<std::uint32_t>(4U) * pulsesPerQuarterNote;
    const std::uint32_t pulseDenominator =
        configuration_->beatUnit != 0U ? configuration_->beatUnit : 4U;
    const std::uint64_t pulseIntervalQ32 = core::calculateNextIntervalQ32(
        pulseNumerator,
        pulseDenominator,
        externalPulseRemainder_);
    const std::uint64_t expectedPulsePositionQ32 = externalReferenceQ32_ + pulseIntervalQ32;
    externalMusicalPositionQ32_ += pulseIntervalQ32;

    // The master position stands while STOPPED/PAUSED. Correcting the channel
    // epochs against it would move every schedule one pulse interval into the
    // past per pulse, and PLAY would then catch up as a burst. Only the musical
    // position (display, lock estimator) follows the pulses while stopped.
    if (playing_) {
        alignGlobalSchedulesToExternalPulse(expectedPulsePositionQ32);
    }
    externalReferenceQ32_ = masterPositionQ32_;
    masterBeatPhaseQ32_ = externalMusicalPositionQ32_ % core::kQ32One;

    const std::uint64_t completedBeats = externalMusicalPositionQ32_ / core::kQ32One;
    masterBeatSerial_ = static_cast<std::uint32_t>(completedBeats & 0xFFFFFFFFULL);
    const std::uint8_t beatsPerBar =
        configuration_->beatsPerBar != 0U ? configuration_->beatsPerBar : 1U;
    masterBeat_ = static_cast<std::uint8_t>(completedBeats % beatsPerBar) + 1U;
    masterBar_ = static_cast<std::uint16_t>((completedBeats / beatsPerBar) % 999ULL) + 1U;
}

void ClockEngine::alignGlobalSchedulesToExternalPulse(
    const std::uint64_t expectedPulsePositionQ32) {
    const std::uint64_t observedPulsePositionQ32 = masterPositionQ32_;
    if (expectedPulsePositionQ32 == observedPulsePositionQ32) {
        return;
    }
    ++scheduleGeneration_;

    // Positions only ever move forward (F-069). A late pulse moves the GLOBAL
    // grid later. An early pulse would move it earlier; near the start of the
    // timeline that falls below zero and the unsigned epoch would saturate,
    // leaving every following beat late. Moving the master timeline and every
    // non-GLOBAL schedule later by the same amount is the same relative
    // correction and never subtracts.
    const bool latePulse = observedPulsePositionQ32 > expectedPulsePositionQ32;
    const std::uint64_t deltaQ32 = latePulse
        ? observedPulsePositionQ32 - expectedPulsePositionQ32
        : expectedPulsePositionQ32 - observedPulsePositionQ32;
    if (latePulse) {
        globalScheduleEpochQ32_ += deltaQ32;
    } else {
        masterPositionQ32_ += deltaQ32;
    }
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        const bool global =
            configuration_->channels[channelIndex].common.resetMode == ResetMode::Global;
        if (global != latePulse) {
            continue;
        }
        ChannelRuntime& runtime = channelRuntime_[channelIndex];
        runtime.scheduleEpochQ32 += deltaQ32;
        runtime.nextEventQ32 += deltaQ32;
        // Processed and pending grid slots move with the epoch they belong to.
        runtime.nextEventSlotQ32 += deltaQ32;
        runtime.lastEventSlotQ32 += deltaQ32;
        // Pending ratchet sub-events belong to the same step grid.
        if (runtime.ratchetRemaining != 0U) {
            runtime.nextRatchetQ32 += deltaQ32;
        }
    }
}

}  // namespace clockfw::engine
