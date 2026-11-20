/**
 * @file menu_model_sync.cpp
 * @brief External-sync settings row formatting split from the main menu model.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "ui/menu_model_sync.h"

#include <cstdio>

#include "domain/clock_labels.h"
#include "text_format.h"
#include "ui_text.h"

namespace clockfw::ui {
namespace {

void copySyncText(char* const destination, const std::size_t size, const text::TextId id) {
    text::formatInto(destination, size, "%s", text::get(id));
}

}  // namespace

MenuRow buildSyncMenuRow(const std::uint8_t rowIndex, const ClockState& state) {
    MenuRow row{};
    constexpr text::TextId kLabels[] = {
        text::TextId::Source,
        text::TextId::Ppqn,
        text::TextId::Edge,
        text::TextId::Loss,
        text::TextId::ResetInputMode,
        text::TextId::Filter,
        text::TextId::Smoothing,
        text::TextId::Timeout,
        text::TextId::Autostart};
    if (rowIndex >= sizeof(kLabels) / sizeof(kLabels[0])) {
        return row;
    }
    copySyncText(row.label, sizeof(row.label), kLabels[rowIndex]);
    if (rowIndex == 0U) {
        text::formatInto(row.value, sizeof(row.value), "%s", clockSourceLabel(state.source));
    } else if (rowIndex == 1U) {
        text::formatInto(
            row.value, sizeof(row.value), "%u", state.externalSync.pulsesPerQuarterNote);
    } else if (rowIndex == 2U) {
        copySyncText(
            row.value,
            sizeof(row.value),
            state.externalSync.edge == SyncEdge::Rising ? text::TextId::SyncRise
                                                        : text::TextId::SyncFall);
    } else if (rowIndex == 3U) {
        const text::TextId lossText = state.externalSync.lossMode == SyncLossMode::Stop
            ? text::TextId::TransportStop
            : (state.externalSync.lossMode == SyncLossMode::Freewheel
                   ? text::TextId::SyncFree
                   : text::TextId::SourceInternal);
        copySyncText(row.value, sizeof(row.value), lossText);
    } else if (rowIndex == 4U) {
        copySyncText(
            row.value,
            sizeof(row.value),
            state.externalSync.resetMode == ExternalResetMode::Trigger ? text::TextId::Trigger
                                                                       : text::TextId::GateInput);
    } else if (rowIndex == 5U) {
        text::formatInto(
            row.value,
            sizeof(row.value),
            text::get(text::TextId::MicrosecondsFormat),
            state.externalSync.glitchFilterUs);
    } else if (rowIndex == 6U) {
        const text::TextId smoothingText = state.externalSync.smoothing == SyncSmoothing::Off
            ? text::TextId::Off
            : (state.externalSync.smoothing == SyncSmoothing::Low
                   ? text::TextId::SyncLow
                   : (state.externalSync.smoothing == SyncSmoothing::Medium
                          ? text::TextId::SyncMedium
                          : text::TextId::SyncFull));
        copySyncText(row.value, sizeof(row.value), smoothingText);
    } else if (rowIndex == 7U) {
        text::formatInto(
            row.value,
            sizeof(row.value),
            text::get(text::TextId::MillisecondsFormat),
            state.externalSync.timeoutMs);
    } else {
        copySyncText(
            row.value,
            sizeof(row.value),
            state.externalSync.autostartOnLock ? text::TextId::On : text::TextId::Off);
    }
    return row;
}

}  // namespace clockfw::ui
