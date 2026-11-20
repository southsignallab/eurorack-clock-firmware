/**
 * @file ui_controller_factory_reset.cpp
 * @brief Guarded full factory-reset control flow.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "ui/ui_controller_factory_reset.h"

#include "domain/default_configuration.h"

namespace clockfw::ui {
void UiController::confirmFactoryReset(const std::uint32_t nowMs) {
    if (navigation_.cursor == 0U) {
        openSettingsPage(SettingsPage::Info, 5U);
        return;
    }

    // A full-image Flash commit stalls STM32F4 instruction fetch. Stop transport
    // first so every gate is LOW and no musical timing can be interrupted.
    state_.transport = TransportState::Stopped;
    engine_.stop();

    ClockState factoryState{};
    initializeFactoryDefaults(factoryState);
    factoryState.transport = TransportState::Stopped;

    if (!persistentState_.factoryReset(factoryState)) {
        // Keep the current in-RAM configuration when persistence could not be
        // replaced. The previous committed A/B slot remains power-loss safe.
        openSettingsPage(SettingsPage::Info, 5U);
        return;
    }

    // F-008: the Flash image is now factory state. Drop every RAM copy that
    // could otherwise write old data back or keep playing it: queued Custom
    // Groove records and cached names, both Sequencer banks, and the engine's
    // Groove/Sequencer snapshots. Done only after the image commit succeeded, so
    // a failed reset leaves RAM and Flash consistently on the previous data.
    if (customGrooveStore_ != nullptr) {
        customGrooveStore_->resetToFactory();
    }
    for (std::uint8_t slot = 0U; slot < kCustomGrooveSlotCount; ++slot) {
        engine_.updateCustomGrooveSlot(slot, CustomGroovePattern{}, false);
    }
    if (sequencerPatternStore_ != nullptr) {
        // Same as a boot without a durable bank: P1 of every channel starts from
        // the factory legacy sequence, so the state does not change on the next boot.
        sequencerPatternStore_->resetToFactory();
        // resetToFactory() cleared the durable and dirty flags, the only reasons seeding refuses.
        (void)sequencerPatternStore_->seedLegacyPatternOnes(factoryState, nowMs);
    }
    if (sequencerStepStore_ != nullptr) {
        sequencerStepStore_->resetToFactory();
    }
    reloadSequencerBanksIntoEngine();

    state_ = factoryState;
    engine_.updateConfiguration(state_, true);
    engine_.stop();
    navigation_.highScoreResetAvailable = false;
    navigation_.selectedChannel = 0U;
    navigation_.screen = Screen::Performance;
    navigation_.cursor = 0U;
    navigation_.scrollOffset = 0U;
    navigation_.editing = false;
    invalidate();
}

void UiController::reloadSequencerBanksIntoEngine() {
    if (sequencerPatternStore_ == nullptr) {
        return;
    }
    for (std::uint8_t channel = 0U; channel < kChannelCount; ++channel) {
        const std::uint8_t slot = sequencerPatternStore_->activeSlot(channel);
        engine_.updateSequencerPattern(
            channel, sequencerPatternStore_->pattern(channel, slot), false);
        std::array<SequencerStepMetadataWord, kSequencerMaximumSteps> metadata{};
        if (sequencerStepStore_ != nullptr) {
            sequencerStepStore_->loadPatternWords(channel, slot, metadata);
        }
        engine_.updateSequencerStepMetadata(channel, metadata);
    }
}

}  // namespace clockfw::ui
