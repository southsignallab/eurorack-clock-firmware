/**
 * @file formula1_track.h
 * @brief Pseudo-3D track geometry shared by the Formula 1 game logic and renderer.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstdint>

namespace clockfw::game::formula1 {

/** Screen row of the horizon, in pixels. */
inline constexpr std::int16_t kHorizonY = 12;
/** Screen row where the road leaves the bottom of the play field, in pixels. */
inline constexpr std::int16_t kRoadBottomY = 46;
/** Top row of the player car, in pixels. */
inline constexpr std::int16_t kPlayerY = 38;
/** Width of the player car body, in pixels. */
inline constexpr std::int16_t kPlayerBodyWidth = 12;
/** Height of the player car body, in pixels. */
inline constexpr std::int16_t kPlayerBodyHeight = 8;

/**
 * @brief Clamps value to [low, high].
 * @param value Value to clamp.
 * @param low Lower bound.
 * @param high Upper bound; must not be below low.
 * @return The clamped value.
 */
std::int16_t clampToRange(std::int16_t value, std::int16_t low, std::int16_t high);
/**
 * @brief Left road edge at screen row y for the track position distance, in pixels.
 *
 * Rows outside [kHorizonY, kRoadBottomY] are clamped to that range.
 * @param distance Track position in track units; the curve profile repeats every 16 segments of
 *        180 units.
 * @param y Screen row, pixels.
 * @return Screen column of the left edge, pixels.
 */
std::int16_t roadLeftAt(std::uint32_t distance, std::int16_t y);
/**
 * @brief Right road edge at screen row y for the track position distance, in pixels.
 * @param distance Track position in track units, as for roadLeftAt().
 * @param y Screen row, pixels; clamped to [kHorizonY, kRoadBottomY].
 * @return Screen column of the right edge, pixels.
 */
std::int16_t roadRightAt(std::uint32_t distance, std::int16_t y);
/**
 * @brief Road centre at screen row y for the track position distance, in pixels.
 * @param distance Track position in track units, as for roadLeftAt().
 * @param y Screen row, pixels; clamped to [kHorizonY, kRoadBottomY].
 * @return Screen column midway between both edges, pixels.
 */
std::int16_t roadCenterAt(std::uint32_t distance, std::int16_t y);
/**
 * @brief Road width at screen row y for the track position distance, in pixels.
 * @param distance Track position in track units, as for roadLeftAt().
 * @param y Screen row, pixels; clamped to [kHorizonY, kRoadBottomY].
 * @return Right edge minus left edge, pixels.
 */
std::int16_t roadWidthAt(std::uint32_t distance, std::int16_t y);
/**
 * @brief Centre of lane -1, 0 or +1 at screen row y for the track position distance.
 * @param distance Track position in track units, as for roadLeftAt().
 * @param lane Lane index; lanes are a quarter of the road width apart.
 * @param y Screen row, pixels; clamped to [kHorizonY, kRoadBottomY].
 * @return Screen column of the lane centre, pixels.
 */
std::int16_t laneCenterAt(std::uint32_t distance, std::int8_t lane, std::int16_t y);
/**
 * @brief Perspective width of a traffic car whose top is at screen row y, in pixels.
 * @param y Screen row of the car top, pixels.
 * @return Width, 4..11 pixels.
 */
std::int16_t trafficCarWidthAt(std::int16_t y);
/**
 * @brief Perspective height of a traffic car whose top is at screen row y, in pixels.
 * @param y Screen row of the car top, pixels.
 * @return Height, 3..7 pixels.
 */
std::int16_t trafficCarHeightAt(std::int16_t y);

}  // namespace clockfw::game::formula1
