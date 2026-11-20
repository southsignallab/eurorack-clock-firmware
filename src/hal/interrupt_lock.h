/**
 * @file interrupt_lock.h
 * @brief RAII critical-section primitive for synchronizing foreground code with timer ISRs.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

namespace clockfw::hal {

/**
 * @brief Disables interrupts for one lexical scope and restores them on destruction.
 *
 * The firmware uses this only from foreground code when copying data shared with
 * the scheduler ISR. It must not be nested or constructed inside an ISR.
 */
class InterruptLock final {
  public:
    /** @brief Enters the critical section by disabling interrupts. */
    InterruptLock();

    /** @brief Leaves the critical section by re-enabling interrupts. */
    ~InterruptLock();

    /** @brief Prevents copying a critical-section guard. */
    InterruptLock(const InterruptLock&) = delete;

    /** @brief Prevents copy-assignment of a critical-section guard. */
    InterruptLock& operator=(const InterruptLock&) = delete;

#if defined(CLOCK_HOST_TEST)
    /** @brief Returns how many InterruptLock scopes are open (host tests only). */
    static std::uint32_t activeCountForTest();

    /** @brief Returns how many InterruptLock scopes were opened so far (host tests only). */
    static std::uint32_t constructionCountForTest();

    /** @brief Returns how many were opened while another was open (host tests only). */
    static std::uint32_t nestedCountForTest();
#endif

  private:
    /** PRIMASK value from platform::enterCritical(), passed to exitCritical() on exit. */
    std::uint32_t previousPrimask_ = 0U;
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
    /** Cycle counter at entry, only in instrumented debug builds (W-056). */
    std::uint32_t startCycles_ = 0U;
#endif
};

}  // namespace clockfw::hal
