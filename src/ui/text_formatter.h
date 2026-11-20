/**
 * @file text_formatter.h
 * @brief Compact string formatting helpers for the 128x64 OLED UI.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstddef>

#include "domain/clock_types.h"

namespace clockfw::ui {

/**
 * @brief Formats a channel rate using compact xN, /N, and optional N:D notation.
 * @param settings Channel settings holding the rate.
 * @param output Destination text buffer.
 * @param outputSize Size of @p output in bytes.
 */
void formatRate(const CommonChannelSettings& settings, char* output, std::size_t outputSize);

/**
 * @brief Formats the primary performance-screen value for one channel.
 * @param channel Channel configuration.
 * @param output Destination text buffer.
 * @param outputSize Size of @p output in bytes.
 */
void formatChannelDetail(const ChannelConfig& channel, char* output, std::size_t outputSize);

/**
 * @brief Formats the compact second-line summary shown in a channel overview tile.
 * @param channel Channel configuration.
 * @param output Destination text buffer.
 * @param outputSize Size of @p output in bytes.
 */
void formatChannelSummary(const ChannelConfig& channel, char* output, std::size_t outputSize);

}  // namespace clockfw::ui
