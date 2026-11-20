/**
 * @file settings_renderer.h
 * @brief Rendering of scrollable settings and factory-template lists.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include "domain/clock_types.h"
#include "hal/oled_display.h"
#include "services/persistent_state_service.h"
#include "services/sequencer_pattern_store.h"
#include "ui/menu_model.h"
#include "ui/ui_types.h"

namespace clockfw::ui {

/** @brief Renders compact text-based configuration screens using one consistent visual grammar. */
class SettingsRenderer final {
  public:
    /**
     * @brief Constructs the renderer around the shared OLED abstraction.
     * @param display Display HAL receiving all drawing commands.
     * @param sequencerPatternStore Optional Sequencer 2.0 store for the Sequencer page values.
     */
    explicit SettingsRenderer(
        hal::OledDisplay& display,
        const services::SequencerPatternStore* sequencerPatternStore = nullptr);

    /**
     * @brief Draws the currently selected settings page.
     * @param state Complete application configuration used to build row values.
     * @param navigation Current settings page, cursor, scroll, and edit state.
     * @param diagnostics Live input/output levels for the hardware diagnostics pages.
     */
    void renderSettings(
        const ClockState& state,
        const NavigationState& navigation,
        const DiagnosticSnapshot& diagnostics = {});

    /**
     * @brief Draws the factory-template selection list.
     * @param navigation Current template cursor and scroll state.
     */
    void renderTemplates(const NavigationState& navigation);

    /**
     * @brief Draws the explicit user-preset slot list for load/save operations.
     * @param navigation Current slot cursor, scroll state and slot action.
     * @param persistentState Source of the slot names and occupancy.
     */
    void renderPresetSlots(
        const NavigationState& navigation,
        const services::PersistentStateService& persistentState);

    /**
     * @brief Draws the overwrite confirmation for an occupied user preset slot.
     * @param navigation Selected preset slot and NO/YES cursor.
     * @param persistentState Source of the slot name.
     */
    void renderOverwriteConfirm(
        const NavigationState& navigation,
        const services::PersistentStateService& persistentState);

    /**
     * @brief Draws APPLY TEMPLATE? with the template name and NO/YES (D-09).
     * @param navigation Pending template and NO/YES cursor.
     */
    void renderTemplateApplyConfirm(const NavigationState& navigation);

    /**
     * @brief Draws the guarded confirmation used before clearing the ranked-game Top-100 table.
     * @param navigation NO/YES cursor.
     */
    void renderHighScoreClearConfirm(const NavigationState& navigation);

    /**
     * @brief Draws the guarded confirmation used before erasing all user data.
     * @param navigation NO/YES cursor.
     */
    void renderFactoryResetConfirm(const NavigationState& navigation);

    /**
     * @brief Draws the 16-character encoder-driven preset-name editor and character band.
     * @param navigation Edited name and character position.
     */
    void renderNameEntry(const NavigationState& navigation);

    /**
     * @brief Draws the full value of one read-only settings row in a compact detail popover.
     * @param navigation Popover title and value text.
     */
    void renderInformationPopover(const NavigationState& navigation);

    /**
     * @brief Returns true when a read-only row value cannot fit beside its label.
     * @param label Row label; nullptr yields false.
     * @param value Row value; nullptr or an empty string yields false.
     * @return true when the value is wider than the space right of the label in the small font.
     *
     * The same pixel budget is used by renderSettings(), so controller activation
     * can expose a popover only for values that are actually abbreviated on screen.
     */
    bool informationValueOverflows(const char* label, const char* value);

  private:
    /**
     * @brief Draws the two input states of the diagnostics page (without presenting).
     * @param diagnostics Live levels to show.
     */
    void renderDiagnosticInputs(const DiagnosticSnapshot& diagnostics);

    /**
     * @brief Draws the eight output states of the diagnostics page (without presenting).
     * @param diagnostics Live levels to show.
     */
    void renderDiagnosticOutputs(const DiagnosticSnapshot& diagnostics);

    /**
     * @brief Builds row rowIndex of the open settings page, including live Groove values.
     * @param state Configuration that supplies the row values.
     * @param navigation Open settings page and selected channel.
     * @param rowIndex Row on the page.
     * @return Label and value of the row.
     */
    MenuRow settingsRow(
        const ClockState& state,
        const NavigationState& navigation,
        std::uint8_t rowIndex) const;

    /**
     * @brief Draws one settings row at y; selected inverts it, editing re-inverts the value.
     * @param row Label and value to draw.
     * @param y Top edge of the row text, in pixels.
     * @param selected The row carries the cursor.
     * @param editing The selected row is in value-edit mode.
     */
    void drawSettingsRow(const MenuRow& row, std::int16_t y, bool selected, bool editing);

    /**
     * @brief Draws the scrollbar when the page has more rows than fit on the screen.
     * @param itemCount Number of rows on the page; must exceed @p visibleRows (the caller checks).
     * @param visibleRows Number of rows shown at once.
     * @param cursor Selected row; positions the thumb.
     */
    void drawSettingsScrollbar(
        std::uint8_t itemCount,
        std::uint8_t visibleRows,
        std::uint8_t cursor);

    /** Display that receives the drawing commands. */
    hal::OledDisplay& display_;
    /** Optional Sequencer 2.0 store for the Sequencer page values. */
    const services::SequencerPatternStore* sequencerPatternStore_ = nullptr;
};

}  // namespace clockfw::ui
