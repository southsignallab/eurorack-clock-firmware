/**
 * @file groove_catalog.h
 * @brief Read-only Stage-1 factory groove definitions and timing helpers.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "domain/clock_types.h"

namespace clockfw::groove {

/** @brief Number of selectable values including OFF. */
inline constexpr std::size_t kPresetCount = 9U;

/**
 * @brief Returns a compact stable UI label for one factory groove.
 * @param preset Groove selection; an out-of-range value gives the OFF label.
 * @return Static label from ui_text.h.
 */
const char* presetLabel(GroovePreset preset);

/**
 * @brief Returns the pattern length in steps; OFF has length one.
 * @param preset Groove selection; an out-of-range value gives the OFF length.
 * @return Length in steps; CUSTOM has length one here.
 */
std::uint8_t patternLength(GroovePreset preset);

/**
 * @brief Returns one deterministic non-negative delay as permille of the nominal event interval.
 * @param preset Factory groove selection.
 * @param eventSerial Zero-based event number on the local channel grid.
 * @param amountPercent User amount from 0..100 percent.
 * @param rotation Pattern rotation in steps.
 * @return Delay in permille of the interval, scaled by the amount; 0 for OFF and CUSTOM.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
std::uint16_t delayPermille(
    GroovePreset preset,
    std::uint64_t eventSerial,
    std::uint8_t amountPercent,
    std::uint8_t rotation);

/**
 * @brief Returns the largest scaled delay in one pattern, in permille.
 * @param preset Groove selection.
 * @param amountPercent Groove Amount in percent; larger values than 100 are clamped.
 * @return Largest delay in permille of the interval, scaled by the amount.
 *
 * @isr TIM3 scheduler interrupt: at most 16 loop rounds, non-blocking, no allocation, opens
 *      no interrupt lock.
 */
std::uint16_t maximumDelayPermille(GroovePreset preset, std::uint8_t amountPercent);

}  // namespace clockfw::groove
