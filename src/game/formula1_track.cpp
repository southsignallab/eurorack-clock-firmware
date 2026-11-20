/**
 * @file formula1_track.cpp
 * @brief Pseudo-3D track geometry of the Formula 1 boot game.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "game/formula1_track.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace clockfw::game::formula1 {
namespace {
constexpr std::int16_t kRoadLeftBottom = 18;
constexpr std::int16_t kRoadRightBottom = 109;
constexpr std::int16_t kRoadLeftTop = 54;
constexpr std::int16_t kRoadRightTop = 73;
constexpr std::uint32_t kTrackSegmentLength = 180U;
constexpr std::array<std::int8_t, 16U> kCurveProfile{
    {0, 0, 4, 9, 14, 10, 5, 0, -5, -11, -15, -10, -4, 0, 5, 0}};

std::int16_t baseInterpolate(const std::int16_t y, const std::int16_t a, const std::int16_t b) {
    const std::int16_t clampedY = clampToRange(y, kHorizonY, kRoadBottomY);
    const std::int32_t numerator =
        static_cast<std::int32_t>(clampedY - kHorizonY) * static_cast<std::int32_t>(b - a);
    return static_cast<std::int16_t>(a + numerator / (kRoadBottomY - kHorizonY));
}
std::int16_t curveStrengthAt(const std::uint32_t distance) {
    const std::size_t index =
        static_cast<std::size_t>((distance / kTrackSegmentLength) % kCurveProfile.size());
    const std::size_t next = (index + 1U) % kCurveProfile.size();
    const std::uint32_t local = distance % kTrackSegmentLength;
    return static_cast<std::int16_t>(
        static_cast<std::int32_t>(kCurveProfile[index]) +
        (static_cast<std::int32_t>(kCurveProfile[next]) -
         static_cast<std::int32_t>(kCurveProfile[index])) *
            static_cast<std::int32_t>(local) / static_cast<std::int32_t>(kTrackSegmentLength));
}
std::int16_t curveOffsetAt(const std::uint32_t distance, const std::int16_t y) {
    const std::int16_t clampedY = clampToRange(y, kHorizonY, kRoadBottomY);
    const std::int32_t remaining = static_cast<std::int32_t>(kRoadBottomY - clampedY);
    const std::int32_t span = static_cast<std::int32_t>(kRoadBottomY - kHorizonY);
    return static_cast<std::int16_t>(
        static_cast<std::int32_t>(curveStrengthAt(distance)) * remaining * remaining /
        (span * span));
}
}  // namespace

std::int16_t clampToRange(
    const std::int16_t value,
    const std::int16_t low,
    const std::int16_t high) {
    return static_cast<std::int16_t>(std::clamp<int>(value, low, high));
}
std::int16_t roadLeftAt(const std::uint32_t distance, const std::int16_t y) {
    return static_cast<std::int16_t>(
        baseInterpolate(y, kRoadLeftTop, kRoadLeftBottom) + curveOffsetAt(distance, y));
}
std::int16_t roadRightAt(const std::uint32_t distance, const std::int16_t y) {
    return static_cast<std::int16_t>(
        baseInterpolate(y, kRoadRightTop, kRoadRightBottom) + curveOffsetAt(distance, y));
}
std::int16_t roadCenterAt(const std::uint32_t distance, const std::int16_t y) {
    return static_cast<std::int16_t>((roadLeftAt(distance, y) + roadRightAt(distance, y)) / 2);
}
std::int16_t roadWidthAt(const std::uint32_t distance, const std::int16_t y) {
    return static_cast<std::int16_t>(roadRightAt(distance, y) - roadLeftAt(distance, y));
}
std::int16_t laneCenterAt(
    const std::uint32_t distance,
    const std::int8_t lane,
    const std::int16_t y) {
    const std::int16_t spacing = static_cast<std::int16_t>(roadWidthAt(distance, y) / 4);
    return static_cast<std::int16_t>(
        roadCenterAt(distance, y) + static_cast<std::int16_t>(lane) * spacing);
}
std::int16_t trafficCarWidthAt(const std::int16_t y) {
    return static_cast<std::int16_t>(4 + std::clamp<int>((y - kHorizonY) / 7, 0, 7));
}
std::int16_t trafficCarHeightAt(const std::int16_t y) {
    return static_cast<std::int16_t>(3 + std::clamp<int>((y - kHorizonY) / 10, 0, 4));
}

}  // namespace clockfw::game::formula1
