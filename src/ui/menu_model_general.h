/**
 * @file menu_model_general.h
 * @brief Rows of the screensaver, information and license settings pages.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstdint>

#include "domain/clock_types.h"
#include "ui/menu_model.h"

namespace clockfw::ui {

/**
 * @brief Fills row rowIndex of the screensaver page (mode and the three delays).
 * @param row Row to fill.
 * @param rowIndex Zero-based row index.
 * @param state Configuration with the display preferences.
 */
void fillScreensaverRow(MenuRow& row, std::uint8_t rowIndex, const ClockState& state);

/**
 * @brief Fills row rowIndex of the information page (product, firmware, author, links).
 * @param row Row to fill.
 * @param rowIndex Zero-based row index.
 */
void fillInfoRow(MenuRow& row, std::uint8_t rowIndex);

/**
 * @brief Fills row rowIndex of the license page (firmware and third-party licenses).
 * @param row Row to fill.
 * @param rowIndex Zero-based row index.
 */
void fillLicensesRow(MenuRow& row, std::uint8_t rowIndex);

}  // namespace clockfw::ui
