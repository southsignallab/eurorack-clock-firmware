/**
 * @file runtime_instrumentation.cpp
 * @brief Debug-build counters: TIM3 ISR, InterruptLock, deferred rises, catch-up, swap fallbacks.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "hal/runtime_instrumentation.h"

#include "config.h"

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
    gClockRuntimeCounters.schedulerCatchUpLimitHits = 0U;
    gClockRuntimeCounters.caughtUpSchedulerTicks = 0U;
    gClockRuntimeCounters.stagedSwapFallbacks = 0U;
    gClockRuntimeCounters.synthesizedInputEdges = 0U;
    gClockRuntimeCounters.encoderAnchors = 0U;
    gClockRuntimeCounters.encoderAnchorRequestPhase = 0U;
    gClockRuntimeCounters.encoderDiscardedRemainders = 0U;
    // SystemCoreClock (CMSIS system_stm32f4xx.h) holds the core clock set by HAL_RCC_ClockConfig().
    const std::uint32_t cyclesPerUs = SystemCoreClock / 1000000U;
    gClockRuntimeCounters.schedulerIsrBudgetCycles = config::kSchedulerIsrBudgetUs * cyclesPerUs;
    gClockRuntimeCounters.interruptLockBudgetCycles = config::kInterruptLockBudgetUs * cyclesPerUs;
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
std::uint32_t cycleCount() {
    return 0U;
}
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

void countSchedulerCatchUpLimit() {
    gClockRuntimeCounters.schedulerCatchUpLimitHits =
        gClockRuntimeCounters.schedulerCatchUpLimitHits + 1U;
}

void countCaughtUpSchedulerTicks(const std::uint32_t elapsedTicks) {
    if (elapsedTicks > 1U) {
        gClockRuntimeCounters.caughtUpSchedulerTicks =
            gClockRuntimeCounters.caughtUpSchedulerTicks + (elapsedTicks - 1U);
    }
}

void countStagedSwapFallback() {
    gClockRuntimeCounters.stagedSwapFallbacks = gClockRuntimeCounters.stagedSwapFallbacks + 1U;
}

void countSynthesizedInputEdge() {
    gClockRuntimeCounters.synthesizedInputEdges = gClockRuntimeCounters.synthesizedInputEdges + 1U;
}

void countEncoderAnchor() {
    gClockRuntimeCounters.encoderAnchors = gClockRuntimeCounters.encoderAnchors + 1U;
}

void recordEncoderAnchorRequestPhase(const std::uint8_t phase) {
    gClockRuntimeCounters.encoderAnchorRequestPhase = phase;
}

void countEncoderDiscardedRemainder() {
    gClockRuntimeCounters.encoderDiscardedRemainders =
        gClockRuntimeCounters.encoderDiscardedRemainders + 1U;
}

}  // namespace clockfw::hal::instrumentation
#endif
