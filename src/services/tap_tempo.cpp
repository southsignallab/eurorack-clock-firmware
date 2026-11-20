/**
 * @file tap_tempo.cpp
 * @brief Stateful tap-tempo estimator used by the performance UI.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "services/tap_tempo.h"

#include <algorithm>

#include "config.h"

namespace clockfw::services {
namespace {

constexpr std::uint32_t kMicrosecondsPerMillisecond = 1000UL;
constexpr std::uint32_t kTapMinimumIntervalUs =
    config::kTapMinimumIntervalMs * kMicrosecondsPerMillisecond;
constexpr std::uint32_t kTapMaximumIntervalUs =
    config::kTapMaximumIntervalMs * kMicrosecondsPerMillisecond;
constexpr std::uint64_t kMicrosecondsPerMinute = 60000000ULL;

// Four 60-s intervals must fit the 32-bit sum.
static_assert(4ULL * kTapMaximumIntervalUs <= UINT32_MAX, "tap interval sum overflows");

}  // namespace

std::uint32_t TapTempo::maximumSequenceIntervalMs(const std::uint16_t minimumBpm) {
    if (minimumBpm == 0U) {
        return 0U;
    }

    const std::uint32_t intervalMs = 60000UL / minimumBpm;
    return std::clamp<std::uint32_t>(
        intervalMs, config::kTapMinimumIntervalMs, config::kTapMaximumIntervalMs);
}

std::uint32_t TapTempo::maximumSequenceIntervalUs(const std::uint16_t minimumBpm) {
    return maximumSequenceIntervalMs(minimumBpm) * kMicrosecondsPerMillisecond;
}

void TapTempo::reset() {
    previousTapAtUs_ = 0U;
    havePreviousTap_ = false;
    intervalsUs_.fill(0U);
    intervalCount_ = 0U;
}

std::uint16_t TapTempo::registerTap(
    const std::uint32_t nowUs,
    const std::uint16_t minimumBpm,
    const std::uint16_t maximumBpm) {
    if (minimumBpm == 0U || maximumBpm == 0U || minimumBpm > maximumBpm) {
        return 0U;
    }
    const std::uint32_t maximumSequenceIntervalUs = TapTempo::maximumSequenceIntervalUs(minimumBpm);
    // Unsigned 32-bit difference: correct across the timer wrap for any interval < 2^32 us.
    if (!havePreviousTap_ || nowUs - previousTapAtUs_ > maximumSequenceIntervalUs) {
        intervalCount_ = 0U;
        previousTapAtUs_ = nowUs;
        havePreviousTap_ = true;
        return 0U;
    }

    const std::uint32_t intervalUs = nowUs - previousTapAtUs_;
    previousTapAtUs_ = nowUs;

    // Reject implausible taps before they can contaminate the rolling average.
    if (intervalUs < kTapMinimumIntervalUs || intervalUs > kTapMaximumIntervalUs) {
        intervalCount_ = 0U;
        return 0U;
    }

    if (intervalCount_ < intervalsUs_.size()) {
        intervalsUs_[intervalCount_++] = intervalUs;
    } else {
        for (std::size_t index = 0U; index + 1U < intervalsUs_.size(); ++index) {
            intervalsUs_[index] = intervalsUs_[index + 1U];
        }
        intervalsUs_.back() = intervalUs;
    }

    std::uint32_t sumUs = 0U;
    for (std::uint8_t index = 0U; index < intervalCount_; ++index) {
        sumUs += intervalsUs_[index];
    }

    if (intervalCount_ == 0U || sumUs == 0U) {
        return 0U;
    }
    const std::uint64_t roundedBpm = (kMicrosecondsPerMinute * intervalCount_ + sumUs / 2U) / sumUs;
    const std::uint64_t clampedBpm = std::clamp<std::uint64_t>(roundedBpm, minimumBpm, maximumBpm);
    return static_cast<std::uint16_t>(clampedBpm);
}

}  // namespace clockfw::services
