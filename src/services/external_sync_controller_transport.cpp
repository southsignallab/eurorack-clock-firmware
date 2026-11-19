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
    engine_.playFromIsr();
    pendingTransportTransition_ = static_cast<std::uint8_t>(TransportState::Playing);
}

void ExternalSyncController::stopFromIsr() {
    deferredStart_ = false;
    engine_.stopFromIsr();
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
    {
        hal::InterruptLock interruptLock;
        if (!deferredStart_) {
            commitWindowOpen_ = false;
            return;
        }
    }
    // F-075: the start is prepared outside any lock. The engine swaps it in under its
    // own lock and, in that same lock, the callback closes the window and takes the
    // deferred start. An input STOP in between clears deferredStart_; then the window
    // closes and nothing starts. The callback has applied the outcome either way.
    (void)engine_.playWhen(&ExternalSyncController::takeDeferredStartUnsafe, this);
}

bool ExternalSyncController::takeDeferredStartUnsafe(void* const context) {
    ExternalSyncController& self = *static_cast<ExternalSyncController*>(context);
    self.commitWindowOpen_ = false;
    if (!self.deferredStart_) {
        return false;
    }
    self.deferredStart_ = false;
    self.pendingTransportTransition_ = static_cast<std::uint8_t>(TransportState::Playing);
    return true;
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
    engine_.stopFromIsr();
    startOrDeferFromIsr();
}

}  // namespace clockfw::services
