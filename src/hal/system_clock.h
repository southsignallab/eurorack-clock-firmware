/**
 * @file system_clock.h
 * @brief Thin HAL wrapper around framework-independent monotonic time and delay primitives.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

namespace clockfw::hal {

/** @brief Provides monotonic timing services without exposing MCU/framework calls to higher layers. */
class SystemClock final {
  public:
    /**
     * @brief Returns milliseconds elapsed since MCU startup.
     * @return Milliseconds from platform::milliseconds(), wrapping at 2^32 ms.
     */
    static std::uint32_t milliseconds();

    /**
     * @brief Returns the wrapping 32-bit microsecond timestamp used for edge capture.
     * @return Microseconds from platform::microseconds() (TIM5 counter in production),
     * wrapping at 2^32 us.
     *
     * @isr Runs in the TIM3 scheduler interrupt and the EXTI input-edge interrupts: bounded
     * (one counter read), non-blocking, no allocation, no output, opens no interrupt lock.
     */
    static std::uint32_t microseconds();

    /**
     * @brief Blocks execution for the requested number of milliseconds.
     * @param durationMs Delay duration in milliseconds.
     */
    static void delayMilliseconds(std::uint32_t durationMs);
};

}  // namespace clockfw::hal
