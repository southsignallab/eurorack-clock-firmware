/**
 * @file extended_timebase.h
 * @brief 64-bit extension of the wrapping 32-bit TIM5 microsecond counter.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * Milliseconds and microseconds derive from this one time base (F-046). SysTick
 * stops counting while a Flash operation stalls the CPU; TIM5 is a hardware
 * counter and keeps counting, so no time is lost.
 *
 * Single writer: the scheduler interrupt (TIM3, highest priority) folds the counter
 * delta since its previous call into a 64-bit anchor. Readers in any context take
 * the anchor and the counter without an interrupt lock. A sequence number tells
 * them that the writer ran in between; they then read again. The writer cannot be
 * interrupted by a reader, so one increment per update is enough. Unsigned 32-bit
 * differences are exact across the TIM5 wrap (2^32 us, 71.6 min) as long as the
 * writer runs at least once per wrap period; it runs every 50 us.
 */

#pragma once

#include <cstdint>

namespace clockfw::hal {

/** @brief Wrap-free microsecond time from a wrapping 32-bit counter. */
class ExtendedTimebase final {
  public:
    /** @brief Returns the current 32-bit counter value (TIM5 in production). */
    using CounterReader = std::uint32_t (*)();

    /**
     * @brief Scheduler-ISR only (single writer): folds the counter up to @p counterUs in.
     * @param counterUs Current 32-bit TIM5 counter value.
     */
    void advanceFromIsr(std::uint32_t counterUs);

    /**
     * @brief Returns the 64-bit microsecond time; safe in foreground and interrupt context.
     * @param readCounter Returns the current 32-bit TIM5 counter value.
     *
     * Read order: sequence, anchor, counter, sequence. A writer that runs anywhere
     * before the second sequence read changes it, and the reader repeats. A writer
     * after that read does not matter. The repetition ends: the writer runs once per
     * 50 us, the window is a few instructions.
     */
    std::uint64_t read(CounterReader readCounter) const;

    /** @brief Milliseconds of @p microseconds, wrapping at 2^32 ms like HAL_GetTick(). */
    static std::uint32_t millisecondsFrom(std::uint64_t microseconds);

  private:
    volatile std::uint64_t anchorExtendedUs_ = 0U;
    volatile std::uint32_t anchorCounterUs_ = 0U;
    volatile std::uint32_t sequence_ = 0U;
};

}  // namespace clockfw::hal
