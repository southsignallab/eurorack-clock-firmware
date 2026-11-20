/**
 * @file menu_model.h
 * @brief Static settings-page metadata and row formatting.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "domain/clock_types.h"
#include "domain/sequencer_pattern.h"
#include "ui/ui_types.h"

namespace clockfw::ui {

/** @brief One rendered settings row with bounded text and optional read-only detail semantics. */
struct MenuRow {
    /** NUL-terminated row label. */
    char label[18]{};
    /** NUL-terminated row value; empty for rows without a value. */
    char value[48]{};
    /** True when the value is informational and may open a full-text detail popover. */
    bool expandableInformation = false;
};

/** Row of HI-SCORES CLEAR on the settings root page while a ranked game has a record (F-038). */
inline constexpr std::uint8_t kRootHighScoresRow = 5U;

/**
 * @brief Returns the compact title for a settings page.
 * @param page Settings page.
 * @return NUL-terminated title from the UI text table; the root title for unknown pages.
 */
const char* settingsPageTitle(SettingsPage page);

/**
 * @brief Returns the number of selectable rows on a settings page.
 * @param page Settings page.
 * @param mode Mode of the selected channel; decides the row count of the Channel root page.
 * @param highScoreResetAvailable Adds the HI-SCORES CLEAR row to the root page.
 * @return Row count of the page.
 */
std::uint8_t settingsPageItemCount(
    SettingsPage page,
    ChannelMode mode = ChannelMode::Clock,
    bool highScoreResetAvailable = false);

/**
 * @brief Returns true when the page is scoped to the currently selected channel.
 * @param page Settings page.
 * @return true for channel-scoped pages.
 */
bool isChannelSettingsPage(SettingsPage page);

/**
 * @brief Formats one settings row from the current application state.
 * @param page Settings page being rendered.
 * @param rowIndex Zero-based row index.
 * @param selectedChannel Zero-based selected channel.
 * @param state Complete clock state.
 * @param highScoreResetAvailable The root page shows the HI-SCORES CLEAR row.
 * @return Formatted row; fields are empty when the row is invalid.
 */
MenuRow buildMenuRow(
    SettingsPage page,
    std::uint8_t rowIndex,
    std::uint8_t selectedChannel,
    const ClockState& state,
    bool highScoreResetAvailable = false);

/**
 * @brief Formats one Sequencer 2.0 settings row from an active pattern snapshot.
 * @param rowIndex Zero-based row index; 0..5 are valid.
 * @param activeSlot Zero-based active pattern slot; shown one-based.
 * @param pattern Active pattern snapshot.
 * @return Formatted row; fields are empty for rows from 6 on.
 */
MenuRow buildSequencerV2MenuRow(
    std::uint8_t rowIndex,
    std::uint8_t activeSlot,
    const SequencerPatternV2& pattern);

/**
 * @brief Returns the index of the current integer rate option.
 * @param settings Channel settings holding the rate.
 * @return Index into the rate option table; a fixed fallback index when no option matches.
 */
std::size_t findRateOptionIndex(const CommonChannelSettings& settings);

/**
 * @brief Returns the index of the current gate-length option.
 * @param gateLengthMs Gate length in milliseconds.
 * @return Index into the gate-length option table; a fixed fallback index when no option
 *         matches.
 */
std::size_t findGateLengthOptionIndex(std::uint16_t gateLengthMs);

/**
 * @brief Returns the index of the current meter-unit option.
 * @param beatUnit Meter unit: note value of one beat (2, 4, 8 or 16 in the option table).
 * @return Index into the meter-unit option table; a fixed fallback index when no option
 *         matches.
 */
std::size_t findBeatUnitOptionIndex(std::uint8_t beatUnit);

}  // namespace clockfw::ui
