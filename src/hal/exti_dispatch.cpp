/**
 * @file exti_dispatch.cpp
 * @brief Pending-line dispatch and settled level reads for shared EXTI interrupt vectors.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/exti_dispatch.h"

namespace clockfw::hal::exti {

void dispatchPendingLines(
    const std::uint32_t firstLine,
    const std::uint32_t lastLine,
    const PendingReader isPending,
    const LineHandler handle) {
    // Bit index, not a shifted 16-bit mask: 0x8000 << 1 wraps to 0 and never ends (F-024).
    for (std::uint32_t line = firstLine; line <= lastLine; ++line) {
        const std::uint32_t lineMask = 1U << line;
        if (isPending(lineMask)) {
            handle(lineMask);
        }
    }
}

bool readSettledLevel(
    const std::uint32_t pin,
    const std::uint32_t lineMask,
    const LevelReader readLevel,
    const PendingReader isPending,
    const PendingClearer clearPending) {
    bool high = readLevel(pin);
    for (std::uint32_t reread = 0U; reread < kMaximumLevelRereads && isPending(lineMask);
         ++reread) {
        clearPending(lineMask);
        high = readLevel(pin);
    }
    return high;
}

}  // namespace clockfw::hal::exti
