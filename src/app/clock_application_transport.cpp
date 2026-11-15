/**
 * @file clock_application_transport.cpp
 * @brief Foreground transport reconciliation and persistence servicing of the CLOCK composition root.
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

void ClockApplication::servicePersistence(const std::uint32_t nowMs) {
    (void)persistenceCoordinator_.service(nowMs, flashWriteAllowed());
    // W-311: once no retry is left, every front-panel action brings SAVE ERROR
    // back for the normal notice time until the stored data changes again.
    const std::uint32_t controlActivity = uiController_.controlActivityRevision();
    const bool remindSaveError = persistenceCoordinator_.retriesExhausted() &&
        persistenceCoordinator_.status() == services::PersistenceCoordinator::Status::SaveError &&
        controlActivity != remindedControlActivity_;
    remindedControlActivity_ = controlActivity;
    if (persistenceCoordinator_.noticeRevision() == announcedPersistenceNotice_ && !remindSaveError) {
        return;
    }
    announcedPersistenceNotice_ = persistenceCoordinator_.noticeRevision();
    uiController_.showPersistenceNotice(
        persistenceCoordinator_.status() == services::PersistenceCoordinator::Status::SaveLocked
            ? ui::PersistenceNotice::SaveLocked
            : ui::PersistenceNotice::SaveError,
        nowMs);
}

#ifdef CLOCK_HOST_TEST
TransportState ClockApplication::transportForTest() const {
    return state_.transport;
}

std::uint16_t ClockApplication::bpmForTest() const {
    return state_.bpm;
}

bool ClockApplication::enginePlayingForTest() const {
    return engine_.isPlaying();
}
#endif

#ifdef CLOCK_SIMULATOR
bool ClockApplication::persistenceSnapshotForSimulator(
    std::array<std::uint8_t, hal::PersistentStorage::kCapacityBytes>& image) {
    return persistenceCoordinator_.snapshotImageForSimulator(image);
}

std::uint64_t ClockApplication::persistenceSnapshotRevisionForSimulator() const {
    return persistenceCoordinator_.snapshotRevisionForSimulator();
}
#endif
}  // namespace clockfw::app
