/**
 * @file gpio_pin.h
 * @brief Framework-independent STM32 GPIO pin identifiers used by the CLOCK HAL.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once
#include <cstdint>

namespace clockfw::mcu {
/**
 * @brief Encoded GPIO pin: port code in bits 8..15 (0xA = GPIOA, 0xB = GPIOB, 0xC = GPIOC),
 * line number in bits 0..3.
 */
using Pin = std::uint32_t;
/** @brief GPIOA line 0; the following declarators name GPIOA lines 1..10. */
inline constexpr Pin PA0 = 0xA00U, PA1 = 0xA01U, PA2 = 0xA02U, PA3 = 0xA03U, PA4 = 0xA04U,
                     PA5 = 0xA05U, PA6 = 0xA06U, PA7 = 0xA07U, PA8 = 0xA08U, PA9 = 0xA09U,
                     PA10 = 0xA0AU;
/** @brief GPIOB line 0; the following declarators name GPIOB lines 1..10 and 12..15. */
inline constexpr Pin PB0 = 0xB00U, PB1 = 0xB01U, PB2 = 0xB02U, PB3 = 0xB03U, PB4 = 0xB04U,
                     PB5 = 0xB05U, PB6 = 0xB06U, PB7 = 0xB07U, PB8 = 0xB08U, PB9 = 0xB09U,
                     PB10 = 0xB0AU, PB12 = 0xB0CU, PB13 = 0xB0DU, PB14 = 0xB0EU, PB15 = 0xB0FU;
/** @brief Marks a signal without a pin; no port code matches it. */
inline constexpr Pin kUnassigned = UINT32_MAX;
/** @var PA1
 * @brief GPIOA line 1. */
/** @var PA2
 * @brief GPIOA line 2. */
/** @var PA3
 * @brief GPIOA line 3. */
/** @var PA4
 * @brief GPIOA line 4. */
/** @var PA5
 * @brief GPIOA line 5. */
/** @var PA6
 * @brief GPIOA line 6. */
/** @var PA7
 * @brief GPIOA line 7. */
/** @var PA8
 * @brief GPIOA line 8. */
/** @var PA9
 * @brief GPIOA line 9. */
/** @var PA10
 * @brief GPIOA line 10. */
/** @var PB1
 * @brief GPIOB line 1. */
/** @var PB2
 * @brief GPIOB line 2. */
/** @var PB3
 * @brief GPIOB line 3. */
/** @var PB4
 * @brief GPIOB line 4. */
/** @var PB5
 * @brief GPIOB line 5. */
/** @var PB6
 * @brief GPIOB line 6. */
/** @var PB7
 * @brief GPIOB line 7. */
/** @var PB8
 * @brief GPIOB line 8. */
/** @var PB9
 * @brief GPIOB line 9. */
/** @var PB10
 * @brief GPIOB line 10. */
/** @var PB12
 * @brief GPIOB line 12. */
/** @var PB13
 * @brief GPIOB line 13. */
/** @var PB14
 * @brief GPIOB line 14. */
/** @var PB15
 * @brief GPIOB line 15. */
}  // namespace clockfw::mcu
