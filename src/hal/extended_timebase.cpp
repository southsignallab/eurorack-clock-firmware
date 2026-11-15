/**
 * @file extended_timebase.cpp
 * @brief 64-bit extension of the wrapping 32-bit TIM5 microsecond counter.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/extended_timebase.h"

namespace clockfw::hal {

void ExtendedTimebase::advanceFromIsr(const std::uint32_t counterUs) {
    // Unsigned difference: exact across the 32-bit wrap.
    const std::uint32_t deltaUs = counterUs - anchorCounterUs_;
    anchorExtendedUs_ = anchorExtendedUs_ + deltaUs;
    anchorCounterUs_ = counterUs;
    sequence_ = sequence_ + 1U;
}

std::uint64_t ExtendedTimebase::read(const CounterReader readCounter) const {
    for (;;) {
        const std::uint32_t sequence = sequence_;
        const std::uint64_t anchorExtendedUs = anchorExtendedUs_;
        const std::uint32_t anchorCounterUs = anchorCounterUs_;
        const std::uint32_t counterUs = readCounter();
        if (sequence == sequence_) {
            return anchorExtendedUs + static_cast<std::uint32_t>(counterUs - anchorCounterUs);
        }
    }
}

std::uint32_t ExtendedTimebase::millisecondsFrom(const std::uint64_t microseconds) {
    return static_cast<std::uint32_t>(microseconds / 1000ULL);
}

}  // namespace clockfw::hal
