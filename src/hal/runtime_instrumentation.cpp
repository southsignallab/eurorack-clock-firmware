/**
 * @file runtime_instrumentation.cpp
 * @brief Debug-build cycle counters for TIM3 ISR runtime, InterruptLock duration and deferred gate rises.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "hal/runtime_instrumentation.h"

#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
#if !defined(CLOCK_HOST_TEST) && !defined(CLOCK_SIMULATOR)
#include <stm32f4xx_hal.h>
#endif

extern "C" {
volatile ClockRuntimeCounters gClockRuntimeCounters = {};
}

namespace clockfw::hal::instrumentation {
#if !defined(CLOCK_HOST_TEST) && !defined(CLOCK_SIMULATOR)
void begin() {
    // core_cm4.h: CoreDebug->DEMCR.TRCENA enables the DWT; DWT->CTRL.CYCCNTENA starts CYCCNT.
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gClockRuntimeCounters.maxSchedulerIsrCycles = 0U;
    gClockRuntimeCounters.maxInterruptLockCycles = 0U;
    gClockRuntimeCounters.deferredGateRises = 0U;
    gClockRuntimeCounters.schedulerIsrSamples = 0U;
    const std::uint32_t first = cycleCount();
    const std::uint32_t second = cycleCount();
    gClockRuntimeCounters.measurementOverheadCycles = second - first;
    gClockRuntimeCounters.cycleCounterRunning = second != first ? 1U : 0U;
}

std::uint32_t cycleCount() {
    return DWT->CYCCNT;
}

void recordInterruptLock(const std::uint32_t startCycles) {
    // Only foreground locks count against the D-13 G-3 limit; IPSR is 0 in thread mode.
    if (__get_IPSR() != 0U) {
        return;
    }
    const std::uint32_t cycles = cycleCount() - startCycles;
    if (cycles > gClockRuntimeCounters.maxInterruptLockCycles) {
        gClockRuntimeCounters.maxInterruptLockCycles = cycles;
    }
}
#else
void begin() {}
std::uint32_t cycleCount() { return 0U; }
void recordInterruptLock(const std::uint32_t) {}
#endif

void recordSchedulerIsr(const std::uint32_t startCycles) {
    // Unsigned difference: correct across one CYCCNT wrap (51 s at 84 MHz).
    const std::uint32_t cycles = cycleCount() - startCycles;
    if (cycles > gClockRuntimeCounters.maxSchedulerIsrCycles) {
        gClockRuntimeCounters.maxSchedulerIsrCycles = cycles;
    }
    gClockRuntimeCounters.schedulerIsrSamples = gClockRuntimeCounters.schedulerIsrSamples + 1U;
}

void countDeferredGateRise() {
    gClockRuntimeCounters.deferredGateRises = gClockRuntimeCounters.deferredGateRises + 1U;
}

}  // namespace clockfw::hal::instrumentation
#endif
