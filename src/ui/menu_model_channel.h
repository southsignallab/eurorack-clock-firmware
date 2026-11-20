/**
 * @file menu_model_channel.h
 * @brief Compact mode-aware channel settings hierarchy.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

#include "domain/clock_types.h"

namespace clockfw::ui {

struct MenuRow;

/** @brief One selectable entry on the Channel root page. */
enum class ChannelMenuAction : std::uint8_t { Mode, Timing, Clock, Euclid, Sequencer, Output };

/**
 * @brief Returns the selectable row count for one Independent channel root.
 * @param mode Channel mode.
 * @return 1 for OFF (MODE only), otherwise 4.
 */
std::uint8_t channelMenuItemCount(ChannelMode mode);

/**
 * @brief Maps one Channel root row to its semantic destination.
 * @param mode Channel mode; decides which rows exist.
 * @param rowIndex Zero-based row on the Channel root page.
 * @return Destination of the row; ChannelMenuAction::Mode for row 0, OFF and unknown rows.
 */
ChannelMenuAction channelMenuAction(ChannelMode mode, std::uint8_t rowIndex);

/**
 * @brief Finds the row carrying one root action, or zero when unavailable.
 * @param mode Channel mode.
 * @param action Destination to look for.
 * @return Zero-based row index, or 0 when the mode has no such row.
 */
std::uint8_t channelMenuIndexForAction(ChannelMode mode, ChannelMenuAction action);

/**
 * @brief Formats one row of the compact mode-aware Channel root page.
 * @param rowIndex Zero-based row index.
 * @param channel Configuration of the selected channel.
 * @return Formatted row.
 */
MenuRow buildChannelMenuRow(std::uint8_t rowIndex, const ChannelConfig& channel);

/**
 * @brief Formats one row of the selected channel Timing group.
 * @param rowIndex Zero-based row index.
 * @param channel Configuration of the selected channel.
 * @return Formatted row.
 */
MenuRow buildChannelTimingMenuRow(std::uint8_t rowIndex, const ChannelConfig& channel);

/**
 * @brief Formats one row of the selected channel Output group.
 * @param rowIndex Zero-based row index.
 * @param channel Configuration of the selected channel.
 * @return Formatted row.
 */
MenuRow buildChannelOutputMenuRow(std::uint8_t rowIndex, const ChannelConfig& channel);

/**
 * @brief Formats one row of the One Clock root group page.
 * @param rowIndex Zero-based row index.
 * @return Formatted row.
 */
MenuRow buildUnifiedClockMenuRow(std::uint8_t rowIndex);

/**
 * @brief Formats one row of the One Clock Timing group.
 * @param rowIndex Zero-based row index.
 * @param settings Shared One Clock settings.
 * @return Formatted row.
 */
MenuRow buildUnifiedTimingMenuRow(std::uint8_t rowIndex, const UnifiedClockSettings& settings);

/**
 * @brief Formats one row of the One Clock Output group.
 * @param rowIndex Zero-based row index.
 * @param settings Shared One Clock settings.
 * @return Formatted row.
 */
MenuRow buildUnifiedOutputMenuRow(std::uint8_t rowIndex, const UnifiedClockSettings& settings);

}  // namespace clockfw::ui
