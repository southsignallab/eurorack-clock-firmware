/**
 * @file custom_groove.cpp
 * @brief Validation and deterministic lookup for user-authored groove patterns.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "domain/custom_groove.h"

#include <algorithm>

namespace clockfw {

bool isCustomGroovePatternValid(const CustomGroovePattern& pattern) {
    if (pattern.length == 0U || pattern.length > kCustomGrooveMaximumSteps) {
        return false;
    }
    for (std::size_t index = 0U; index < pattern.length; ++index) {
        if (pattern.offsets256[index] < kCustomGrooveMinimumOffset256 ||
            pattern.offsets256[index] > kCustomGrooveMaximumOffset256) {
            return false;
        }
    }
    return true;
}

std::int16_t customGrooveOffset256(
    const CustomGroovePattern& pattern,
    const std::uint64_t eventSerial,
    const std::uint8_t amountPercent,
    const std::uint8_t rotation) {
    if (!isCustomGroovePatternValid(pattern) || amountPercent == 0U) {
        return 0;
    }
    const std::uint8_t boundedAmount = std::min<std::uint8_t>(amountPercent, 100U);
    const std::uint8_t rotated = static_cast<std::uint8_t>(rotation % pattern.length);
    const std::uint8_t step = static_cast<std::uint8_t>((eventSerial + rotated) % pattern.length);
    return static_cast<std::int16_t>(
        (static_cast<std::int32_t>(pattern.offsets256[step]) * boundedAmount) / 100);
}

namespace {

/** Half of one nominal interval: the scheduler's symmetric clamp, rounded outward. */
constexpr std::int32_t kHalfIntervalQ16 =
    static_cast<std::int32_t>(kCustomGrooveSpacingQ16One / 2U);

/** Q16 scale factor of one 1/256 Custom Groove offset unit. */
constexpr std::int32_t kOffset256ToQ16 =
    static_cast<std::int32_t>(kCustomGrooveSpacingQ16One / 256U);

/**
 * Returns the combined, clamped Swing + Custom Groove offset of one serial in Q16.
 * The Custom term equals customGrooveOffset256() for a pattern validated once by the caller.
 */
std::int32_t combinedOffsetQ16(
    const CustomGroovePattern& pattern,
    const std::uint32_t length,
    const std::uint32_t boundedAmount,
    const std::uint32_t rotation,
    const std::uint32_t eventSerial,
    const std::int32_t oddSwingQ16) {
    const std::uint32_t step = (eventSerial + rotation % length) % length;
    const std::int32_t customQ16 = ((static_cast<std::int32_t>(pattern.offsets256[step]) *
                                     static_cast<std::int32_t>(boundedAmount)) /
                                    100) *
        kOffset256ToQ16;
    const std::int32_t swingQ16 = (eventSerial & 1U) != 0U ? oddSwingQ16 : 0;
    return std::clamp<std::int32_t>(customQ16 + swingQ16, -kHalfIntervalQ16, kHalfIntervalQ16);
}

}  // namespace

std::uint32_t customGrooveMinimumSpacingQ16(
    const CustomGroovePattern& pattern,
    const std::uint8_t amountPercent,
    const std::uint8_t rotation,
    const std::uint8_t swingPercent) {
    // The scheduler floors Swing to its Q32 grid. Overestimating the earlier event's
    // delay and underestimating the later event's delay keeps the result conservative.
    const std::uint32_t boundedSwing = std::min<std::uint32_t>(swingPercent, 50U);
    const std::int32_t swingLowQ16 =
        static_cast<std::int32_t>((boundedSwing * kCustomGrooveSpacingQ16One) / 100U);
    const std::int32_t swingHighQ16 =
        static_cast<std::int32_t>((boundedSwing * kCustomGrooveSpacingQ16One + 99U) / 100U);

    // An invalid pattern or Amount 0 contributes no Custom offsets, exactly as in
    // customGrooveOffset256(); a neutral one-step pattern models that case.
    static constexpr CustomGroovePattern kNeutralPattern{1U, {}};
    const bool shaped = amountPercent != 0U && isCustomGroovePatternValid(pattern);
    const CustomGroovePattern& source = shaped ? pattern : kNeutralPattern;
    const std::uint32_t length = source.length;
    const std::uint32_t boundedAmount = std::min<std::uint32_t>(amountPercent, 100U);
    // Swing alternates every event, so odd pattern lengths repeat after two passes.
    const std::uint32_t cycle = (length & 1U) != 0U ? length * 2U : length;
    const std::int32_t one = static_cast<std::int32_t>(kCustomGrooveSpacingQ16One);

    // Serial 0 cannot move before the epoch, so a negative first offset is clamped to 0.
    const std::int32_t firstQ16 = std::max<std::int32_t>(
        combinedOffsetQ16(source, length, boundedAmount, rotation, 0U, swingHighQ16), 0);
    std::int32_t minimumQ16 = one +
        combinedOffsetQ16(source, length, boundedAmount, rotation, 1U, swingLowQ16) - firstQ16;

    for (std::uint32_t serial = 0U; serial < cycle; ++serial) {
        const std::int32_t earlierQ16 =
            combinedOffsetQ16(source, length, boundedAmount, rotation, serial, swingHighQ16);
        const std::int32_t laterQ16 =
            combinedOffsetQ16(source, length, boundedAmount, rotation, serial + 1U, swingLowQ16);
        minimumQ16 = std::min(minimumQ16, one + laterQ16 - earlierQ16);
    }
    // Both offsets are clamped to +-half an interval, so the spacing cannot be negative.
    return static_cast<std::uint32_t>(minimumQ16);
}

}  // namespace clockfw
