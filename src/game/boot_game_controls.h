/**
 * @file boot_game_controls.h
 * @brief Control timing shared by the boot games.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

namespace clockfw::game {

/** Hold time of the encoder press that leaves a boot game, in milliseconds. */
inline constexpr std::uint32_t kExitLongPressMs = 900U;

}  // namespace clockfw::game
