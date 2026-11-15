/**
 * @file custom_groove.h
 * @brief Bounded runtime representation for user-authored deterministic groove patterns.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace clockfw {

/** Maximum number of editable steps in one Custom Groove. */
inline constexpr std::size_t kCustomGrooveMaximumSteps = 64U;

/** Number of durable Custom Groove slots supported by the current 8-KiB image. */
inline constexpr std::uint8_t kCustomGrooveSlotCount = 10U;

/** Signed offset unit: one value step equals 1/256 of the nominal event interval. */
inline constexpr std::int8_t kCustomGrooveMinimumOffset256 = -120;
inline constexpr std::int8_t kCustomGrooveMaximumOffset256 = 120;

/**
 * @brief One deterministic Custom Groove pattern kept outside ClockState.
 *
 * Offsets are signed fractions of the nominal event interval. The deliberately
 * bounded range leaves at least 1/16 of one straight interval between two
 * adjacent maximally opposed markers before Swing composition is applied.
 */
struct CustomGroovePattern final {
    std::uint8_t length = 16U;
    std::array<std::int8_t, kCustomGrooveMaximumSteps> offsets256{};
};

/** @brief Returns true when one pattern obeys the durable/runtime bounds. */
bool isCustomGroovePatternValid(const CustomGroovePattern& pattern);

/** @brief Returns one clamped signed offset for the selected serial/rotation. */
std::int16_t customGrooveOffset256(
    const CustomGroovePattern& pattern,
    std::uint64_t eventSerial,
    std::uint8_t amountPercent,
    std::uint8_t rotation);

/** One nominal event interval in the Q16 event-spacing scale. */
inline constexpr std::uint32_t kCustomGrooveSpacingQ16One = 65536U;

/**
 * @brief Returns the smallest distance between two adjacent shaped Custom Groove events.
 *
 * Mirrors the scheduler's Custom Groove shaping: signed offsets scaled by Amount and
 * rotated, the odd-event Swing delay added, and the combined offset clamped to half an
 * interval. The first interval after the epoch, where a negative offset is clamped to
 * the epoch, is included. Rounding is conservative: the scheduled spacing is never
 * smaller than the returned value. Bounded by 2 x 64 pattern steps; foreground only.
 *
 * @param pattern Durable or preview pattern; an invalid pattern contributes no offsets.
 * @param amountPercent Groove Amount, 0..100 percent (larger values are clamped).
 * @param rotation Pattern rotation in steps.
 * @param swingPercent Channel Swing, 0..50 percent (larger values are clamped).
 * @return Spacing as a fraction of one nominal interval, in units of
 *         1/kCustomGrooveSpacingQ16One; 0..2 x kCustomGrooveSpacingQ16One.
 */
std::uint32_t customGrooveMinimumSpacingQ16(
    const CustomGroovePattern& pattern,
    std::uint8_t amountPercent,
    std::uint8_t rotation,
    std::uint8_t swingPercent);

}  // namespace clockfw
