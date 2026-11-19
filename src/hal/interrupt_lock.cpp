/**
 * @file interrupt_lock.cpp
 * @brief RAII critical-section primitive for synchronizing foreground code with timer ISRs.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/interrupt_lock.h"

#include "hal/platform_io.h"
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
#include "hal/runtime_instrumentation.h"
#endif

namespace clockfw::hal {

#if defined(CLOCK_HOST_TEST)
namespace {
std::uint32_t gActiveLocksForTest = 0U;
std::uint32_t gConstructedLocksForTest = 0U;
std::uint32_t gNestedLocksForTest = 0U;
}  // namespace

std::uint32_t InterruptLock::activeCountForTest() {
    return gActiveLocksForTest;
}

std::uint32_t InterruptLock::constructionCountForTest() {
    return gConstructedLocksForTest;
}

std::uint32_t InterruptLock::nestedCountForTest() {
    return gNestedLocksForTest;
}
#endif

InterruptLock::InterruptLock() {
#if defined(CLOCK_HOST_TEST)
    ++gConstructedLocksForTest;
    if (gActiveLocksForTest != 0U) {
        ++gNestedLocksForTest;
    }
    ++gActiveLocksForTest;
#endif
    previousPrimask_ = platform::enterCritical();
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
    startCycles_ = instrumentation::cycleCount();
#endif
}

InterruptLock::~InterruptLock() {
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
    instrumentation::recordInterruptLock(startCycles_);
#endif
    platform::exitCritical(previousPrimask_);
#if defined(CLOCK_HOST_TEST)
    --gActiveLocksForTest;
#endif
}

}  // namespace clockfw::hal
