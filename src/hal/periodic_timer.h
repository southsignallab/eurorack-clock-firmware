/**
 * @file periodic_timer.h
 * @brief HAL wrapper for the STM32 TIM3 scheduler interrupt.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once
#include <cstdint>
#if defined(CLOCK_HOST_TEST) || defined(CLOCK_SIMULATOR)
#include <HardwareTimer.h>
#endif
namespace clockfw::hal {
class PeriodicTimer final {
  public:
    using Callback = void (*)();
    /** @brief Constructs the TIM3 scheduler wrapper. */
    PeriodicTimer();
    /** @brief Starts periodic TIM3 interrupts at the requested frequency. */
    void start(std::uint32_t frequencyHz, Callback callback);
    /** @brief Stops TIM3 scheduler interrupts. */
    void stop();
    /**
     * @brief ISR-only: microseconds since the latest TIM3 update event (TIM3 counter).
     *
     * TIM3 counts the same 1 MHz timer clock as TIM5, so subtracting this value from a
     * TIM5 timestamp yields the timestamp of the update. Host and simulator builds fire
     * the scheduler exactly on the update and return 0.
     */
    static std::uint32_t elapsedSinceUpdateUsFromIsr();

  private:
#if defined(CLOCK_HOST_TEST) || defined(CLOCK_SIMULATOR)
    HardwareTimer timer_;
#endif
};
}  // namespace clockfw::hal
