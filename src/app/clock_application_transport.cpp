/**
 * @file clock_application_transport.cpp
 * @brief Foreground transport reconciliation and the Flash-commit gate of the CLOCK composition root.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "app/clock_application.h"

namespace clockfw::app {
void ClockApplication::adoptExternalTransportTransition() {
    TransportState externalTransport{};
    if (!externalSyncController_.consumeTransportTransition(externalTransport)) {
        return;
    }
    // The ISR queues only its latest transition. A front-panel action later in
    // the previous pass may already have overtaken it (e.g. RUN starts the
    // engine, then STOP stops it). Accept the transition only while it still
    // matches the engine, so the display never shows PLAY for a stopped engine
    // or STOP for a running one.
    const bool transitionIsPlaying = externalTransport == TransportState::Playing;
    if (transitionIsPlaying == engine_.isPlaying()) {
        state_.transport = externalTransport;
    }
    uiController_.invalidate();
}

bool ClockApplication::flashWriteAllowed() const {
    return state_.transport != TransportState::Playing && !engine_.isPlaying();
}

#ifdef CLOCK_HOST_TEST
TransportState ClockApplication::transportForTest() const {
    return state_.transport;
}

bool ClockApplication::enginePlayingForTest() const {
    return engine_.isPlaying();
}
#endif
}  // namespace clockfw::app
