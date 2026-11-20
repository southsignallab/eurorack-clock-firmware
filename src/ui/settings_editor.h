/**
 * @file settings_editor.h
 * @brief Validated mutation service for user-editable clock and channel settings.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "domain/clock_types.h"
#include "engine/clock_engine.h"
#include "services/custom_groove_store.h"
#include "ui/ui_types.h"

namespace clockfw::ui {

/**
 * @brief Applies validated settings mutations and synchronizes committed changes to the engine.
 *
 * Navigation is intentionally not handled here. The editor knows what a value means,
 * while UiController decides which screen/row is currently active.
 */
class SettingsEditor final {
  public:
    /**
     * @brief Constructs the editor around mutable application state and the real-time engine.
     * @param state Mutable musical/application state.
     * @param engine Real-time engine receiving committed changes.
     * @param customGrooveStore Custom Groove library; without it, Groove PRESET never offers
     *        CUSTOM.
     */
    SettingsEditor(
        ClockState& state,
        engine::ClockEngine& engine,
        const services::CustomGrooveStore* customGrooveStore = nullptr);

    /**
     * @brief Changes the master tempo by one or more encoder detents.
     * @param delta Signed encoder detent count.
     */
    void changeMasterTempo(std::int8_t delta);

    /**
     * @brief Adjusts one settings value according to page and row semantics.
     * @param page Active settings page.
     * @param rowIndex Selected row on the page.
     * @param channelIndex Selected channel for channel-scoped pages.
     * @param delta Signed encoder detent count.
     */
    void adjust(
        SettingsPage page,
        std::uint8_t rowIndex,
        std::uint8_t channelIndex,
        std::int8_t delta);

    /**
     * @brief Toggles one gate bit in a sequencer pattern.
     * @param channelIndex Selected channel.
     * @param absoluteStep Zero-based absolute step in the 64-step pattern.
     */
    void toggleSequencerStep(std::uint8_t channelIndex, std::uint8_t absoluteStep);

    /**
     * @brief Executes a non-numeric sequencer command.
     * @param channelIndex Selected channel.
     * @param rowIndex Sequencer-settings row containing the command.
     * @return True when a command was recognized and executed.
     */
    bool executeSequencerCommand(std::uint8_t channelIndex, std::uint8_t rowIndex);

  private:
    /**
     * @brief Adjusts one external-input role assignment.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustInputAssignments(std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one device-local hardware preference row.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustHardware(std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one master-settings row.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustMaster(std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one external-sync settings row.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustSync(std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts STOP-mode screensaver and OLED timeout preferences.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustScreensaver(std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one common channel-settings row.
     * @param channelIndex Selected channel.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustCommonChannel(std::uint8_t channelIndex, std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one rate-settings row.
     * @param channelIndex Selected channel.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustRate(std::uint8_t channelIndex, std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one CLOCK-specific settings row.
     * @param channelIndex Selected channel.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustClock(std::uint8_t channelIndex, std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one EUCLID-specific settings row.
     * @param channelIndex Selected channel.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustEuclid(std::uint8_t channelIndex, std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one SEQ-specific settings row.
     * @param channelIndex Selected channel.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustSequencer(std::uint8_t channelIndex, std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one shared eight-output Timing setting.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustUnifiedTiming(std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts one shared eight-output Output setting.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustUnifiedOutput(std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Adjusts the active global/per-channel Stage-1 Groove assignment.
     * @param channelIndex Selected channel; ignored in the One Clock operating mode, which
     *        edits the shared Groove.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustGroove(std::uint8_t channelIndex, std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Returns the first Custom Groove slot that holds a groove (F-037).
     * @param slotIndex Receives the slot.
     * @return False when no slot holds a groove or no library is attached.
     */
    bool firstStoredCustomGroove(std::uint8_t& slotIndex) const;

    /**
     * @brief Adjusts one fixed divider-bank setting.
     * @param rowIndex Selected row on the page.
     * @param delta Signed encoder detent count.
     */
    void adjustDividerBank(std::uint8_t rowIndex, std::int8_t delta);

    /**
     * @brief Clamps a signed integer to an inclusive range.
     * @param value Value to clamp.
     * @param minimum Inclusive lower bound.
     * @param maximum Inclusive upper bound.
     * @return @p value limited to [minimum, maximum].
     */
    static int clampInt(int value, int minimum, int maximum);

    /** Application state edited by the settings pages; owned by the caller. */
    ClockState& state_;
    /** Real-time engine that receives committed changes. */
    engine::ClockEngine& engine_;
    /** Custom Groove library; nullptr means CUSTOM is never offered. */
    const services::CustomGrooveStore* customGrooveStore_ = nullptr;
    /** Gate bits copied by the legacy Sequencer COPY command. */
    std::uint64_t sequencerClipboard_ = 0U;
    /** sequencerClipboard_ holds a copied pattern. */
    bool sequencerClipboardValid_ = false;
};

}  // namespace clockfw::ui
