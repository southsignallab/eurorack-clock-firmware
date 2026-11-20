/**
 * @file menu_model_groove.cpp
 * @brief Formatting helper for Stage-1 Groove settings.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "ui/menu_model_groove.h"
#include "domain/groove_catalog.h"
#include "text_format.h"
#include "ui_text.h"
#include <cstdio>
namespace clockfw::ui {
MenuRow buildGrooveMenuRow(
    const std::uint8_t rowIndex,
    const std::uint8_t selectedChannel,
    const ClockState& state) {
    MenuRow row{};
    const GrooveSettings& settings = state.operatingMode == OperatingMode::UnifiedClock
        ? state.unifiedClock.groove
        : state.channels[selectedChannel].common.groove;
    if (rowIndex == 0U) {
        text::formatInto(row.label, sizeof(row.label), "%s", text::get(text::TextId::GroovePreset));
        text::formatInto(row.value, sizeof(row.value), "%s", groove::presetLabel(settings.preset));
    } else if (rowIndex == 1U) {
        text::formatInto(row.label, sizeof(row.label), "%s", text::get(text::TextId::Amount));
        text::formatInto(row.value, sizeof(row.value), "%u%%", settings.amountPercent);
    } else if (rowIndex == 2U) {
        text::formatInto(row.label, sizeof(row.label), "%s", text::get(text::TextId::Rotate));
        if (settings.preset == GroovePreset::Custom) {
            text::formatInto(row.value, sizeof(row.value), "%u", settings.rotation);
        } else {
            const std::uint8_t length = groove::patternLength(settings.preset);
            text::formatInto(
                row.value,
                sizeof(row.value),
                "%u/%u",
                settings.rotation,
                length > 0U ? length - 1U : 0U);
        }
    } else if (rowIndex == 3U) {
        text::formatInto(row.label, sizeof(row.label), "%s", text::get(text::TextId::Load));
        text::formatInto(row.value, sizeof(row.value), "%s", text::get(text::TextId::Arrow));
    } else if (rowIndex == 4U) {
        text::formatInto(row.label, sizeof(row.label), "%s", text::get(text::TextId::Editor));
        text::formatInto(row.value, sizeof(row.value), "%s", text::get(text::TextId::Arrow));
    } else if (rowIndex == 5U) {
        text::formatInto(row.label, sizeof(row.label), "%s", text::get(text::TextId::Record));
        text::formatInto(row.value, sizeof(row.value), "%s", text::get(text::TextId::Arrow));
    } else if (rowIndex == 6U) {
        text::formatInto(row.label, sizeof(row.label), "%s", text::get(text::TextId::Rename));
        text::formatInto(row.value, sizeof(row.value), "%s", text::get(text::TextId::Arrow));
    } else if (rowIndex == 7U) {
        text::formatInto(row.label, sizeof(row.label), "%s", text::get(text::TextId::Delete));
        text::formatInto(row.value, sizeof(row.value), "%s", text::get(text::TextId::Arrow));
    }
    return row;
}
}  // namespace clockfw::ui
