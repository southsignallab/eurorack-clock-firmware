/**
 * @file clock_application_simulator.cpp
 * @brief Simulator-only lifecycle: non-blocking boot, boot Easter eggs and test probes.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "app/clock_application.h"

#ifdef CLOCK_SIMULATOR

#include <cstdint>

#include "config.h"
#include "hal/system_clock.h"

namespace clockfw::app {

const ClockState& ClockApplication::stateForSimulator() const {
    return state_;
}

const hal::OledDisplay& ClockApplication::displayForSimulator() const {
    return display_;
}

engine::EngineSnapshot ClockApplication::engineSnapshotForSimulator() const {
    return engine_.snapshot();
}

void ClockApplication::injectExternalSyncLevelForSimulator(const bool high) {
    externalInputs_.injectSyncEdgeForTest(hal::SystemClock::microseconds(), high);
}

void ClockApplication::clearExternalSyncForSimulator() {
    externalSyncController_.clearExternalLock();
}

void ClockApplication::injectExternalResetLevelForSimulator(const bool high) {
    externalInputs_.injectResetEdgeForTest(hal::SystemClock::microseconds(), high);
}

bool ClockApplication::runningForSimulator() const {
    return simulatorLifecycle_ == SimulatorLifecycle::Running;
}

bool ClockApplication::easterEggActiveForSimulator() const {
    return simulatorLifecycle_ == SimulatorLifecycle::EasterEgg;
}

bool ClockApplication::gateOutputEnabledForSimulator() const {
    return gateOutputs_.outputStageEnabled();
}

void ClockApplication::powerOffForSimulator() {
    schedulerTimer_.stop();
    gateOutputs_.setAllChannelsLow();
    gateOutputs_.disableOutputStage();
    display_.clear();
    display_.present();
    simulatorLifecycle_ = SimulatorLifecycle::PoweredOff;
    activeInstance_ = nullptr;
}

bool ClockApplication::openGeneralSettingsForSimulator() {
    if (simulatorLifecycle_ != SimulatorLifecycle::Running) {
        return false;
    }
    uiController_.openGeneralSettingsForHost();
    return true;
}

const ui::NavigationState& ClockApplication::navigationForSimulator() const {
    return uiController_.navigation();
}

void ClockApplication::serviceSimulatorStartup(const std::uint32_t nowMs) {
    if (simulatorLifecycle_ == SimulatorLifecycle::PoweredOff) {
        return;
    }
    if (simulatorLifecycle_ == SimulatorLifecycle::EasterEgg) {
        if (!serviceSelectedEasterEggForSimulator(nowMs)) {
            finishSimulatorStartup();
        }
        return;
    }

    std::uint32_t elapsedMs = nowMs - simulatorBootStartedAtMs_;
    if (elapsedMs > config::kBootDurationMs) {
        elapsedMs = config::kBootDurationMs;
    }
    simulatorEncoderHeldForEntireBoot_ =
        simulatorEncoderHeldForEntireBoot_ && controlPanel_.sample(nowMs).encoderButton.pressed;

    if (simulatorLastBootRenderAtMs_ == UINT32_MAX ||
        elapsedMs - simulatorLastBootRenderAtMs_ >= config::kBootRefreshIntervalMs ||
        elapsedMs == config::kBootDurationMs) {
        renderer_.renderBootScreen(elapsedMs);
        simulatorLastBootRenderAtMs_ = elapsedMs;
    }
    if (elapsedMs < config::kBootDurationMs) {
        return;
    }

    if (simulatorEncoderHeldForEntireBoot_) {
        if (selectedLeaderboard_ != nullptr) {
            (void)selectedLeaderboard_->markStarted();  // a failed marker only hides HI-SCORES
        }
        simulatorLifecycle_ = SimulatorLifecycle::EasterEgg;
        beginSelectedEasterEggForSimulator();
        return;
    }
    finishSimulatorStartup();
}

void ClockApplication::finishSimulatorStartup() {
    engine_.begin(state_);
    loadSequencerPatternBank();
    externalSyncController_.begin(state_);
    schedulerTimer_.start(config::kSchedulerFrequencyHz, schedulerInterruptThunk);
    engine_.enableOutputStage();
    uiController_.invalidate();
    simulatorLifecycle_ = SimulatorLifecycle::Running;
}

void ClockApplication::beginSelectedEasterEggForSimulator() {
    switch (config::kEasterEgg) {
        case config::EasterEgg::Formula1:
            formula1Game_.beginForSimulator();
            break;
        case config::EasterEgg::Breakout:
            breakoutGame_.beginForSimulator();
            break;
        case config::EasterEgg::EggJourney:
            eggJourneyGame_.beginForSimulator();
            break;
        case config::EasterEgg::Beatknecht:
            beatknecht_.beginForSimulator();
            break;
        case config::EasterEgg::PixelRaid:
            pixelRaidGame_.beginForSimulator();
            break;
        default:
            beatknecht_.beginForSimulator();
            break;
    }
}

bool ClockApplication::serviceSelectedEasterEggForSimulator(const std::uint32_t nowMs) {
    switch (config::kEasterEgg) {
        case config::EasterEgg::Formula1:
            return formula1Game_.serviceForSimulator(nowMs);
        case config::EasterEgg::Breakout:
            return breakoutGame_.serviceForSimulator(nowMs);
        case config::EasterEgg::EggJourney:
            return eggJourneyGame_.serviceForSimulator(nowMs);
        case config::EasterEgg::Beatknecht:
            return beatknecht_.serviceForSimulator(nowMs);
        case config::EasterEgg::PixelRaid:
            return pixelRaidGame_.serviceForSimulator(nowMs);
        default:
            return beatknecht_.serviceForSimulator(nowMs);
    }
}

}  // namespace clockfw::app

#endif
