/**
 * @file external_sync_controller_transport.cpp
 * @brief Input-driven transport requests and the Flash commit window of ExternalSyncController (D-04).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "services/external_sync_controller.h"

#include "hal/interrupt_lock.h"

namespace clockfw::services {

void ExternalSyncController::startOrDeferFromIsr() {
    if (commitWindowOpen_) {
        // The CPU is about to stall on Flash erase/program. Starting now would
        // emit the first gates with wrong timing; start once the window closes.
        deferredStart_ = true;
        return;
    }
    engine_.play();
    pendingTransportTransition_ = static_cast<std::uint8_t>(TransportState::Playing);
}

void ExternalSyncController::stopFromIsr() {
    deferredStart_ = false;
    engine_.stop();
    pendingTransportTransition_ = static_cast<std::uint8_t>(TransportState::Stopped);
}

bool ExternalSyncController::tryOpenCommitWindow() {
    hal::InterruptLock interruptLock;
    if (engine_.isPlaying()) {
        return false;
    }
    commitWindowOpen_ = true;
    return true;
}

void ExternalSyncController::closeCommitWindow() {
    hal::InterruptLock interruptLock;
    commitWindowOpen_ = false;
    if (deferredStart_) {
        deferredStart_ = false;
        engine_.play();
        pendingTransportTransition_ = static_cast<std::uint8_t>(TransportState::Playing);
    }
}

void ExternalSyncController::requestPlayFromInput() {
    autoTransportArmed_ = true;
    startOrDeferFromIsr();
}

void ExternalSyncController::requestStopFromInput() {
    autoTransportArmed_ = false;
    stopFromIsr();
}

void ExternalSyncController::requestRestartFromInput() {
    autoTransportArmed_ = true;
    engine_.stop();
    startOrDeferFromIsr();
}

}  // namespace clockfw::services
