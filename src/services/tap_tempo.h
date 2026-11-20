/**
 * @file tap_tempo.h
 * @brief Stateful tap-tempo estimator used by the performance UI.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstdint>

namespace clockfw::services {

/** @brief Estimates BPM from up to four recent valid tap intervals. */
class TapTempo final {
  public:
    /** @brief Clears all stored taps and interval history. */
    void reset();

    /**
     * @brief Returns the longest interval that still belongs to one Tap Tempo sequence.
     * @param minimumBpm Current user-configured lower BPM limit.
     * @return Maximum valid inter-tap interval in milliseconds, or 0 for an invalid limit.
     *
     * The sequence timeout follows the slowest tempo the user currently permits. A tap
     * arriving later than this becomes the first tap of a new sequence instead of being
     * clamped to the minimum BPM.
     */
    static std::uint32_t maximumSequenceIntervalMs(std::uint16_t minimumBpm);

    /**
     * @brief Microsecond form of maximumSequenceIntervalMs() for the tap time base.
     * @param minimumBpm Current user-configured lower BPM limit.
     * @return Maximum valid inter-tap interval in microseconds, or 0 for an invalid limit.
     */
    static std::uint32_t maximumSequenceIntervalUs(std::uint16_t minimumBpm);

    /**
     * @brief Processes one tap timestamp.
     *
     * Panel and input taps must share one wrapping 32-bit microsecond time base (TIM5).
     * Intervals are unsigned differences, so the 2^32-us wrap (~71.6 min) is transparent.
     *
     * @param nowUs Tap time in microseconds.
     * @param minimumBpm Lower BPM clamp.
     * @param maximumBpm Upper BPM clamp.
     * @return New BPM when enough valid timing data exists, otherwise 0.
     */
    std::uint16_t registerTap(
        std::uint32_t nowUs,
        std::uint16_t minimumBpm,
        std::uint16_t maximumBpm);

  private:
    std::uint32_t previousTapAtUs_ = 0U;
    bool havePreviousTap_ = false;
    std::array<std::uint32_t, 4U> intervalsUs_{};
    std::uint8_t intervalCount_ = 0U;
};

}  // namespace clockfw::services
