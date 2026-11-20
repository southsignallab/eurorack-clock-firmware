/**
 * @file gate_output_driver.h
 * @brief HAL driver for the eight gate/LED logic outputs and optional buffer-enable line.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace clockfw::hal {

/** @brief Owns all physical gate-output GPIO operations. */
class GateOutputDriver final {
  public:
    /**
     * @brief Configures output GPIOs and leaves logical gate output muted.
     *
     * This method guarantees that all eight logic signals are LOW before the
     * firmware may allow gate HIGH requests.
     */
    void beginDisabled();

    /** @brief Enables logical gate output after all source signals are known LOW. */
    void enableOutputStage();

    /** @brief Immediately disables logical gate output and forces all source GPIOs LOW. */
    void disableOutputStage();

    /** @brief Drives every channel source signal LOW. */
    void setAllChannelsLow();

    /**
     * @brief Updates one gate/LED source signal.
     * @param channelIndex Zero-based channel index in the range 0..7.
     * @param high True for an active gate, false for LOW.
     *
     * Out-of-range indices are ignored; HIGH is written only while the output stage is
     * enabled.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    void setChannelState(std::size_t channelIndex, bool high);

    /**
     * @brief Writes all eight gate/LED source signals in one GPIOB register access.
     * @param highMask Bit n requests channel n HIGH; cleared bits drive LOW.
     *
     * All channels go LOW while the output stage is disabled.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    void setChannelStates(std::uint8_t highMask);

    /**
     * @brief Records one channel level for the next commitStagedChannels(); scheduler ISR only.
     *
     * A later call for the same channel replaces the earlier one, so a channel is
     * never in both the set and the reset mask.
     *
     * @param channelIndex Zero-based channel index in the range 0..7; others are ignored.
     * @param high True for an active gate; staged as LOW while the output stage is disabled.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    void stageChannelState(std::size_t channelIndex, bool high);

    /**
     * @brief Writes every staged level in exactly one GPIOB BSRR access and clears the stage.
     *
     * Called once at the end of each scheduler tick, also when nothing is staged.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    void commitStagedChannels();

    /**
     * @brief Returns the last source level written for one gate channel.
     * @param channelIndex Zero-based channel index in the range 0..7.
     * @return True when the channel was last written HIGH; false for LOW or an index out of
     * range.
     */
    bool channelStateHigh(std::size_t channelIndex) const;

    /**
     * @brief Returns whether the logical gate-output stage currently accepts HIGH requests.
     * @return True between enableOutputStage() and the next disableOutputStage() or
     * beginDisabled().
     */
    bool outputStageEnabled() const;

  private:
    /**
     * @brief Writes channel bit masks as GPIOB lines and updates the cached levels.
     * @param setChannels Bit n drives channel n HIGH.
     * @param resetChannels Bit n drives channel n LOW unless it is also in @p setChannels.
     *
     * Only lines whose cached level changes are written, in one writePortLines() call.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    void writeChannelMasks(std::uint8_t setChannels, std::uint8_t resetChannels);

    /** True while HIGH requests reach the GPIOs. */
    bool outputStageEnabled_{false};
    /** Last written source level per channel, bit n = channel n HIGH. */
    std::uint8_t channelHighMask_{0U};
    /** Channels staged HIGH for the next commitStagedChannels(), bit n = channel n. */
    std::uint8_t stagedSetChannels_{0U};
    /** Channels staged LOW for the next commitStagedChannels(), bit n = channel n. */
    std::uint8_t stagedResetChannels_{0U};
};

}  // namespace clockfw::hal
