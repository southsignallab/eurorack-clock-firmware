/**
 * @file ui_controller_tempo.cpp
 * @brief Manual tempo gestures (Tap Tempo) and the external-tempo ownership notice.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "ui/ui_controller.h"

#include "config.h"
#include "services/tap_tempo.h"

namespace clockfw::ui {

bool UiController::masterTempoIsExternal() const {
    if (state_.source == ClockSource::External) {
        return true;
    }
    return state_.source == ClockSource::Auto && engine_.snapshot().externalLocked;
}

void UiController::showExternalTempoNotice(const std::uint32_t nowMs) {
    externalTempoNoticeShownAtMs_ = nowMs;
    if (!navigation_.externalTempoNotice) {
        navigation_.externalTempoNotice = true;
    }
    invalidate();
}

void UiController::serviceExternalTempoNotice(const std::uint32_t nowMs) {
    if (navigation_.externalTempoNotice &&
        (navigation_.screen != Screen::Performance ||
         nowMs - externalTempoNoticeShownAtMs_ >= config::kExternalTempoNoticeMs)) {
        navigation_.externalTempoNotice = false;
        invalidate();
    }
}

void UiController::showPersistenceNotice(const PersistenceNotice notice, const std::uint32_t nowMs) {
    navigation_.persistenceNotice = notice;
    persistenceNoticeShownAtMs_ = nowMs;
    invalidate();
}

void UiController::servicePersistenceNotice(const std::uint32_t nowMs) {
    if (navigation_.persistenceNotice != PersistenceNotice::None &&
        nowMs - persistenceNoticeShownAtMs_ >= config::kPersistenceNoticeMs) {
        navigation_.persistenceNotice = PersistenceNotice::None;
        invalidate();
    }
}

void UiController::registerExternalTapTempo(const std::uint32_t timestampMs) {
    // A TAP input cannot override an external clock either; ignore it silently.
    if (masterTempoIsExternal()) {
        return;
    }
    registerTapTempo(timestampMs);
}

void UiController::registerTapTempo(const std::uint32_t nowMs) {
    if (masterTempoIsExternal()) {
        showExternalTempoNotice(nowMs);
        return;
    }
    const std::uint32_t sequenceIntervalMs =
        services::TapTempo::maximumSequenceIntervalMs(state_.tempoRange.minimumBpm);
    const bool sequenceContinues =
        tapVisualSequenceActive_ && sequenceIntervalMs > 0U &&
        nowMs - lastTapVisualAtMs_ <= sequenceIntervalMs;

    lastTapVisualAtMs_ = nowMs;
    tapVisualSequenceActive_ = sequenceIntervalMs > 0U;
    if (sequenceContinues) {
        tapIndicatorStartedAtMs_ = nowMs;
        navigation_.tapIndicatorFrame = 1U;
        invalidate();
    } else {
        navigation_.tapIndicatorFrame = 0U;
    }

    const std::uint16_t estimatedBpm = tapTempo_.registerTap(
        nowMs, state_.tempoRange.minimumBpm, state_.tempoRange.maximumBpm);
    if (estimatedBpm == 0U) {
        return;
    }

    state_.bpm = estimatedBpm;
    // Tap Tempo updates only the internal/fallback BPM; SOURCE remains user-selected.
    engine_.updateConfiguration(state_, false);
    persistCurrentState(nowMs);
    invalidate();
}

}  // namespace clockfw::ui
