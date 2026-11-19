/**
 * @file settings_editor_groove.cpp
 * @brief Stage-1 Groove settings mutations.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "ui/settings_editor.h"
#include "domain/groove_catalog.h"
namespace clockfw::ui {
void SettingsEditor::adjustGroove(
    const std::uint8_t channelIndex,
    const std::uint8_t rowIndex,
    const std::int8_t delta) {
    GrooveSettings& settings = state_.operatingMode == OperatingMode::UnifiedClock
        ? state_.unifiedClock.groove
        : state_.channels[channelIndex].common.groove;
    if (rowIndex == 0U) {
        // CUSTOM is offered only while a Custom Groove slot holds a groove (E-13, F-037).
        std::uint8_t storedSlot = 0U;
        const bool customAvailable = firstStoredCustomGroove(storedSlot);
        const int lastPreset = customAvailable
            ? static_cast<int>(GroovePreset::Custom)
            : static_cast<int>(GroovePreset::Custom) - 1;
        const GroovePreset previous = settings.preset;
        settings.preset = static_cast<GroovePreset>(clampInt(
            static_cast<int>(settings.preset) + delta,
            0,
            lastPreset));
        if (settings.preset == GroovePreset::Custom && previous != GroovePreset::Custom &&
            !customGrooveStore_->exists(settings.customSlot)) {
            settings.customSlot = storedSlot;
        }
        const std::uint8_t length = settings.preset == GroovePreset::Custom
            ? kCustomGrooveMaximumSteps
            : groove::patternLength(settings.preset);
        if (settings.rotation >= length) settings.rotation = 0U;
    } else if (rowIndex == 1U) {
        settings.amountPercent = static_cast<std::uint8_t>(clampInt(
            static_cast<int>(settings.amountPercent) + delta, 0, 100));
    } else if (rowIndex == 2U) {
        const std::uint8_t length = settings.preset == GroovePreset::Custom
            ? kCustomGrooveMaximumSteps
            : groove::patternLength(settings.preset);
        settings.rotation = static_cast<std::uint8_t>(clampInt(
            static_cast<int>(settings.rotation) + delta, 0, static_cast<int>(length - 1U)));
    } else {
        return;
    }
    if (state_.operatingMode == OperatingMode::UnifiedClock) {
        engine_.updateConfiguration(state_, true);
    } else {
        engine_.updateChannel(channelIndex, state_.channels[channelIndex], true);
    }
}

bool SettingsEditor::firstStoredCustomGroove(std::uint8_t& slotIndex) const {
    if (customGrooveStore_ == nullptr) {
        return false;
    }
    for (std::uint8_t slot = 0U; slot < kCustomGrooveSlotCount; ++slot) {
        if (customGrooveStore_->exists(slot)) {
            slotIndex = slot;
            return true;
        }
    }
    return false;
}
}  // namespace clockfw::ui
