/**
 * @file clock_engine.cpp
 * @brief Deterministic shared scheduler for all eight gate channels.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "engine/clock_engine.h"

#include "clock_core.h"
#include "config.h"
#include "engine/output_mode_resolver.h"
#include "hal/interrupt_lock.h"

namespace clockfw::engine {
namespace {

/** Events one channel may process in one scheduler quantum. */
constexpr std::uint8_t kMaximumChannelEventsPerQuantum = 8U;
static_assert(
    config::kMaximumSchedulerCatchUpTicks <= kMaximumChannelEventsPerQuantum,
    "Caught-up ticks must not hold more events than one quantum processes");

}  // namespace

ClockEngine::ClockEngine(hal::GateOutputDriver& gateOutputs) : gateOutputs_(gateOutputs) {
    initializeBufferTables();
}

void ClockEngine::initializeBufferTables() {
    for (GrooveSources& table : grooveSourceBuffers_) {
        for (std::size_t slot = 0U; slot < kCustomGrooveSlotCount; ++slot) {
            table.slots[slot] = &customGrooveBuffers_[slot];
        }
        for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
            table.previews[channelIndex] =
                &customGrooveBuffers_[kCustomGrooveSlotCount + channelIndex];
        }
    }
    spareCustomGroove_ = &customGrooveBuffers_[kCustomGrooveBufferCount - 1U];
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        sequencerPatterns_[channelIndex] = &sequencerPatternBuffers_[channelIndex];
        sequencerStepMetadata_[channelIndex] = &sequencerStepMetadataBuffers_[channelIndex];
    }
    spareSequencerPattern_ = &sequencerPatternBuffers_[kChannelCount];
    spareSequencerStepMetadata_ = &sequencerStepMetadataBuffers_[kChannelCount];
}

void ClockEngine::begin(const ClockState& state) {
    std::uint32_t spacingQ16[kChannelCount]{};
    calculateCustomGrooveSpacingsQ16(state, spacingQ16);
    hal::InterruptLock interruptLock;
    copyConfigurationUnsafe(state);
    installCustomGrooveSpacingsUnsafe(spacingQ16);

    globalScheduleEpochQ32_ = masterPositionQ32_;
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        // All channels start from one shared musical epoch. This is essential for
        // phase-coherent CLOCK/EUCLID/SEQ boundaries at equivalent rates.
        channelRuntime_[channelIndex].scheduleEpochQ32 = globalScheduleEpochQ32_;
        // Different deterministic seeds avoid identical probability streams on every output.
        channelRuntime_[channelIndex].randomState ^=
            0x9E3779B9UL * static_cast<std::uint32_t>(channelIndex + 1U);
        channelRuntime_[channelIndex].lastEventSlotValid = false;
        scheduleChannelFromCurrentPosition(channelIndex, true);
    }
    // Startup stays paused until the external output stage is safely enabled.
    playing_ = false;
    restartPending_ = true;
}

void ClockEngine::updateMasterTempo(const std::uint16_t bpm) {
    if (configuration_->bpm == bpm) {
        return;
    }
    hal::InterruptLock interruptLock;
    configuration_->bpm = bpm;
    discardFreewheelTempoAfterManualChangeUnsafe();
}

void ClockEngine::discardFreewheelTempoAfterManualChangeUnsafe() {
    // Decision D-02: with source AUTO and no external signal, a manual tempo
    // change wins over the last measured (freewheel) tempo. A returning signal
    // relocks and owns the tempo again. EXTERNAL keeps its freewheel tempo.
    if (configuration_->source == ClockSource::Auto && !externalLocked_) {
        externalBpmMilli_ = 0U;
    }
}

void ClockEngine::pause() {
    hal::InterruptLock interruptLock;
    playing_ = false;
    restartPending_ = false;
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        ChannelRuntime& runtime = channelRuntime_[channelIndex];
        runtime.tieHold = false;
        runtime.ratchetRemaining = 0U;
        setGateState(channelIndex, false);
    }
}

void ClockEngine::runSchedulerTick(const std::uint32_t elapsedTicks) {
    if (elapsedTicks == 0U) {
        // This TIM3 update was already counted; only the sync service's staged
        // changes are written.
        return;
    }
    schedulerTickCounter_ += elapsedTicks;
    serviceDeferredGateRises();

    // Gate-off checks always run, even while transport is paused or stopped.
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        ChannelRuntime& runtime = channelRuntime_[channelIndex];
        if (runtime.gateHigh && runtime.gateOffScheduled &&
            static_cast<std::int32_t>(schedulerTickCounter_ - runtime.gateOffTick) >= 0) {
            setGateState(channelIndex, false);
        }
    }

    if (externalResetGate_) {
        return;
    }
    if (!playing_) {
        return;
    }
    if (configuration_->source == ClockSource::External && !externalLocked_ &&
        configuration_->syncLossMode == SyncLossMode::Stop) {
        return;
    }

    if (preCountActive_) {
        // Locked external pulses own Pre-Count progress; do not double-advance it.
        const bool drivenByExternalPulses =
            configuration_->source != ClockSource::Internal && externalLocked_;
        if (drivenByExternalPulses) {
            return;
        }
        const core::MasterAdvanceQ32 preCountAdvance = core::advanceMasterPositionMilliBpmQ32(
            effectiveBpmMilli(),
            configuration_->beatUnit,
            config::kSchedulerFrequencyHz,
            elapsedTicks,
            preCountRemainder_);
        preCountPhaseQ32_ += preCountAdvance.totalQ32;
        while (preCountPhaseQ32_ >= core::kQ32One) {
            preCountPhaseQ32_ -= core::kQ32One;
            if (preCountRemaining_ > 0U) {
                --preCountRemaining_;
            }
            if (preCountRemaining_ == 0U) {
                finishPreCountUnsafe();
                break;
            }
        }
        return;
    }

    // Caught-up ticks (F-012) advance the timeline at once; their events fire
    // below in this quantum, once each and in order.
    const core::MasterAdvanceQ32 advance = core::advanceMasterPositionMilliBpmQ32(
        effectiveBpmMilli(),
        configuration_->beatUnit,
        config::kSchedulerFrequencyHz,
        elapsedTicks,
        masterRemainder_);

    masterPositionQ32_ += advance.totalQ32;
    masterBeatPhaseQ32_ += advance.totalQ32;
    while (masterBeatPhaseQ32_ >= core::kQ32One) {
        masterBeatPhaseQ32_ -= core::kQ32One;
        ++masterBeatSerial_;
        ++masterBeat_;

        const std::uint8_t beatsPerBar =
            configuration_->beatsPerBar != 0U ? configuration_->beatsPerBar : 1U;
        if (masterBeat_ > beatsPerBar) {
            masterBeat_ = 1U;
            ++masterBar_;
            if (masterBar_ > 999U) {
                masterBar_ = 1U;
            }
        }
    }

    serviceSequencerRatchets();

    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        if (configuration_->channels[channelIndex].common.mode == ChannelMode::Off) {
            setGateState(channelIndex, false);
            continue;
        }
        std::uint8_t eventGuard = 0U;
        while (masterPositionQ32_ >= channelRuntime_[channelIndex].nextEventQ32 &&
               eventGuard++ < kMaximumChannelEventsPerQuantum) {
            fireChannelEvent(channelIndex);
            ChannelRuntime& runtime = channelRuntime_[channelIndex];
            runtime.lastEventSlotQ32 = runtime.nextEventSlotQ32;
            runtime.lastEventSlotValid = true;
            ++runtime.nextEventSerial;
            runtime.nextEventQ32 = calculateEventPositionQ32(
                channelIndex, runtime.nextEventSerial, &runtime.nextEventSlotQ32);
        }
    }
    endGatesBeforeNextRise(advance.perTickFloorQ32);
    restartPending_ = false;
}

bool ClockEngine::isPlaying() const {
    hal::InterruptLock interruptLock;
    return playing_;
}

EngineSnapshot ClockEngine::snapshot() const {
    EngineSnapshot result{};
    hal::InterruptLock interruptLock;
    result.playing = playing_;
    result.masterPositionQ32 = masterPositionQ32_;
    result.masterBeatPhaseQ32 = masterBeatPhaseQ32_;
    result.masterBeatSerial = masterBeatSerial_;
    result.masterBeat = masterBeat_;
    result.masterBar = masterBar_;
    result.preCountActive = preCountActive_;
    result.preCountRemaining = preCountRemaining_;
    result.preCountPhaseQ32 = preCountPhaseQ32_;
    result.externalLocked = externalLocked_;
    result.externalResetHeld = externalResetGate_;
    result.externalBpmMilli = externalBpmMilli_;
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        result.channelStep[channelIndex] = channelRuntime_[channelIndex].displayedStep;
    }
    return result;
}

#ifdef CLOCK_HOST_TEST
std::uint32_t ClockEngine::resetRuntimeCountForTest() const {
    return resetRuntimeCountForTest_;
}
#endif

void ClockEngine::startPreCountUnsafe() {
    preCountRemaining_ = configuration_->preCountSteps;
    preCountPhaseQ32_ = 0U;
    preCountRemainder_ = 0U;
    preCountExternalPulseRemainder_ = 0U;
    preCountActive_ = preCountRemaining_ != 0U;
}
void ClockEngine::finishPreCountUnsafe() {
    preCountActive_ = false;
    preCountRemaining_ = 0U;
    preCountPhaseQ32_ = 0U;
    preCountRemainder_ = 0U;
    preCountExternalPulseRemainder_ = 0U;
    resetRuntime();
}

void ClockEngine::copyConfigurationUnsafe(const ClockState& state) {
    resolveConfiguration(state, *configuration_);
}

void ClockEngine::resolveConfiguration(
    const ClockState& state,
    EngineConfiguration& configuration) {
    configuration.bpm = state.bpm;
    configuration.beatsPerBar = state.masterMeter.beats;
    configuration.beatUnit = state.masterMeter.unit;
    configuration.preCountSteps = state.preCountSteps;
    configuration.source = state.source;
    configuration.syncLossMode = state.externalSync.lossMode;
    configuration.operatingMode = state.operatingMode;
    configuration.unifiedHumanizeUs =
        state.operatingMode == OperatingMode::UnifiedClock ? state.unifiedClock.humanizeUs : 0U;

    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        configuration.channels[channelIndex] =
            resolvePhysicalOutputConfiguration(state, channelIndex);
    }
}

}  // namespace clockfw::engine
