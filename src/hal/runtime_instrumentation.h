/**
 * @file runtime_instrumentation.h
 * @brief Debug-build cycle counters for TIM3 ISR runtime, InterruptLock duration and deferred gate rises.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * Compiled only with -DCLOCK_RUNTIME_INSTRUMENTATION=1 in a PlatformIO debug
 * build (build_type = debug). Release builds contain none of this code: every
 * caller is guarded by the same macro, and enabling it in a release build stops
 * the compilation below. The counters live in the C symbol
 * gClockRuntimeCounters, so a debugger reads them over SWD without a halt-time
 * function call. No ISR produces output.
 */
#pragma once

#include <cstdint>

#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
#if !defined(__PLATFORMIO_BUILD_DEBUG__) && !defined(CLOCK_HOST_TEST)
#error "CLOCK_RUNTIME_INSTRUMENTATION is only allowed in debug builds (build_type = debug)"
#endif

/** @brief SWD-readable maxima and counts; all values in DWT CYCCNT cycles unless noted. */
struct ClockRuntimeCounters {
    /** Largest observed TIM3_IRQHandler body duration. */
    std::uint32_t maxSchedulerIsrCycles;
    /** Largest observed foreground InterruptLock duration (thread mode only). */
    std::uint32_t maxInterruptLockCycles;
    /** Rising edges moved by one tick because the gate was still HIGH (count, not cycles). */
    std::uint32_t deferredGateRises;
    /** Number of measured TIM3 ISR entries (count). */
    std::uint32_t schedulerIsrSamples;
    /** Cycles between two back-to-back counter reads, measured once at begin(). */
    std::uint32_t measurementOverheadCycles;
    /** Non-zero when CYCCNT advanced during begin(); zero means the counter is not running. */
    std::uint32_t cycleCounterRunning;
    /** Interrupts whose merged updates exceeded kMaximumSchedulerCatchUpTicks (count, F-012). */
    std::uint32_t schedulerCatchUpLimitHits;
    /** Ticks caught up beyond one per interrupt, summed (count, F-012). */
    std::uint32_t caughtUpSchedulerTicks;
};

extern "C" volatile ClockRuntimeCounters gClockRuntimeCounters;

namespace clockfw::hal::instrumentation {

/** @brief Enables the DWT cycle counter and clears all counters; call once at boot. */
void begin();

/** @brief Returns the current DWT CYCCNT value. */
std::uint32_t cycleCount();

/** @brief Records one TIM3 ISR duration that started at @p startCycles. */
void recordSchedulerIsr(std::uint32_t startCycles);

/** @brief Records one InterruptLock duration that started at @p startCycles (thread mode only). */
void recordInterruptLock(std::uint32_t startCycles);

/** @brief Counts one rising edge moved by one scheduler tick. */
void countDeferredGateRise();

/** @brief Counts one scheduler interrupt that dropped updates beyond the catch-up limit. */
void countSchedulerCatchUpLimit();

/** @brief Adds the ticks beyond one that a scheduler interrupt caught up (@p elapsedTicks > 1). */
void countCaughtUpSchedulerTicks(std::uint32_t elapsedTicks);

}  // namespace clockfw::hal::instrumentation
#endif
