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
/**
 * @brief Owns TIM3 as the periodic scheduler interrupt and calls one callback per update.
 *
 * Production builds program TIM3 with a 1 MHz counter clock and the priorities from
 * config.h; host and simulator builds use the HardwareTimer adapter.
 */
class PeriodicTimer final {
  public:
    /** @brief Function called from the TIM3 update interrupt. */
    using Callback = void (*)();
    /** @brief Constructs the TIM3 scheduler wrapper. */
    PeriodicTimer();
    /**
     * @brief Starts periodic TIM3 interrupts at the requested frequency.
     * @param frequencyHz Update rate in Hz; the period is (1 MHz / @p frequencyHz) counts.
     * @param callback Called from the TIM3 interrupt on every update event.
     *
     * Stops a running timer first. If HAL_TIM_Base_Init() fails, the timer stays stopped.
     */
    void start(std::uint32_t frequencyHz, Callback callback);
    /** @brief Stops TIM3 scheduler interrupts. */
    void stop();
    /**
     * @brief ISR-only: microseconds since the latest TIM3 update event (TIM3 counter).
     *
     * TIM3 counts the same 1 MHz timer clock as TIM5, so subtracting this value from a
     * TIM5 timestamp yields the timestamp of the update. Host and simulator builds fire
     * the scheduler exactly on the update and return 0.
     *
     * @return TIM3 counter value in microseconds; 0 in host and simulator builds.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded (one register read), non-blocking,
     * no allocation, no output, opens no interrupt lock.
     */
    static std::uint32_t elapsedSinceUpdateUsFromIsr();

  private:
#if defined(CLOCK_HOST_TEST) || defined(CLOCK_SIMULATOR)
    HardwareTimer timer_;
#endif
};
}  // namespace clockfw::hal
