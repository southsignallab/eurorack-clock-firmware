/**
 * @file gate_output_driver.cpp
 * @brief HAL driver for the eight gate/LED logic outputs and optional shared buffer-enable line.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/gate_output_driver.h"

#include <array>

#include "hal/platform_io.h"

#include "domain/clock_types.h"
#include "pin_map.h"

namespace clockfw::hal {
namespace {

/**
 * @brief Returns the bit of @p channelIndex in a channel mask.
 * @param channelIndex Zero-based channel index in the range 0..7.
 * @return 1 << @p channelIndex.
 *
 * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
 * opens no interrupt lock.
 */
constexpr std::uint8_t channelBit(const std::size_t channelIndex) {
    return static_cast<std::uint8_t>(1U << channelIndex);
}

/** @brief GPIOB line masks for every combination of four channel bits starting at @p firstChannel. */
constexpr std::array<std::uint16_t, 16U> makeGateLineNibbleTable(const std::size_t firstChannel) {
    std::array<std::uint16_t, 16U> table{};
    for (std::size_t channels = 0U; channels < table.size(); ++channels) {
        std::uint16_t lines = 0U;
        for (std::size_t bit = 0U; bit < 4U; ++bit) {
            if ((channels & (1U << bit)) != 0U) {
                lines = static_cast<std::uint16_t>(
                    lines | pinmap::pinLineMask(pinmap::kGateChannelPins[firstChannel + bit]));
            }
        }
        table[channels] = lines;
    }
    return table;
}

// pin_map.h asserts one GPIOB line per gate, so both tables map onto disjoint gate lines.
constexpr std::array<std::uint16_t, 16U> kGateLinesLow = makeGateLineNibbleTable(0U);
constexpr std::array<std::uint16_t, 16U> kGateLinesHigh = makeGateLineNibbleTable(4U);

/**
 * @brief Maps eight channel bits to their GPIOB line mask with two table lookups.
 * @param channels Bit n selects channel n.
 * @return GPIOB line mask of the selected gate pins.
 *
 * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
 * opens no interrupt lock.
 */
std::uint16_t gateLinesFor(const std::uint8_t channels) {
    return static_cast<std::uint16_t>(
        kGateLinesLow[channels & 0x0FU] | kGateLinesHigh[channels >> 4U]);
}

}  // namespace

void GateOutputDriver::beginDisabled() {
    outputStageEnabled_ = false;
    // Level first, then output mode (V3): no pin drives anything but its safe level.
    if (pinmap::kGateBufferOutputEnablePin != pinmap::kUnassignedDigitalPin) {
        platform::configureOutputAtLevel(
            pinmap::kGateBufferOutputEnablePin, pinmap::kGateBufferDisabledLevel);
    }

    for (std::size_t index = 0U; index < pinmap::kGateChannelPins.size(); ++index) {
        platform::configureOutputAtLevel(pinmap::kGateChannelPins[index], false);
    }
    channelHighMask_ = 0U;
    stagedSetChannels_ = 0U;
    stagedResetChannels_ = 0U;
}

void GateOutputDriver::enableOutputStage() {
    // Source GPIOs remain LOW until the engine deliberately raises a channel.
    outputStageEnabled_ = true;
    if (pinmap::kGateBufferOutputEnablePin != pinmap::kUnassignedDigitalPin) {
        platform::write(pinmap::kGateBufferOutputEnablePin, pinmap::kGateBufferEnabledLevel);
    }
}

void GateOutputDriver::disableOutputStage() {
    outputStageEnabled_ = false;
    setAllChannelsLow();
    if (pinmap::kGateBufferOutputEnablePin != pinmap::kUnassignedDigitalPin) {
        platform::write(pinmap::kGateBufferOutputEnablePin, pinmap::kGateBufferDisabledLevel);
    }
}

void GateOutputDriver::setAllChannelsLow() {
    // Safety path: drive every line LOW regardless of the cached levels.
    platform::writePortLines(pinmap::kGateChannelPins[0], 0U, gateLinesFor(0xFFU));
    channelHighMask_ = 0U;
}

void GateOutputDriver::setChannelState(const std::size_t channelIndex, const bool high) {
    if (channelIndex >= kChannelCount) {
        return;
    }

    const bool effectiveHigh = outputStageEnabled_ && high;
    const std::uint8_t bit = channelBit(channelIndex);
    writeChannelMasks(effectiveHigh ? bit : 0U, effectiveHigh ? 0U : bit);
}

void GateOutputDriver::setChannelStates(const std::uint8_t highMask) {
    const std::uint8_t effectiveHigh = outputStageEnabled_ ? highMask : 0U;
    writeChannelMasks(effectiveHigh, static_cast<std::uint8_t>(~effectiveHigh));
}

void GateOutputDriver::stageChannelState(const std::size_t channelIndex, const bool high) {
    if (channelIndex >= kChannelCount) {
        return;
    }
    const std::uint8_t bit = channelBit(channelIndex);
    if (outputStageEnabled_ && high) {
        stagedSetChannels_ = static_cast<std::uint8_t>(stagedSetChannels_ | bit);
        stagedResetChannels_ =
            static_cast<std::uint8_t>(stagedResetChannels_ & ~static_cast<std::uint32_t>(bit));
    } else {
        stagedResetChannels_ = static_cast<std::uint8_t>(stagedResetChannels_ | bit);
        stagedSetChannels_ =
            static_cast<std::uint8_t>(stagedSetChannels_ & ~static_cast<std::uint32_t>(bit));
    }
}

void GateOutputDriver::commitStagedChannels() {
    const std::uint8_t setChannels = stagedSetChannels_;
    const std::uint8_t resetChannels = stagedResetChannels_;
    stagedSetChannels_ = 0U;
    stagedResetChannels_ = 0U;
    writeChannelMasks(setChannels, resetChannels);
}

void GateOutputDriver::writeChannelMasks(
    const std::uint8_t setChannels,
    const std::uint8_t resetChannels) {
    // Only lines whose level changes are written, so the host log shows edges only.
    const std::uint8_t rising =
        static_cast<std::uint8_t>(setChannels & ~static_cast<std::uint32_t>(channelHighMask_));
    const std::uint8_t falling = static_cast<std::uint8_t>(
        static_cast<std::uint32_t>(resetChannels) & channelHighMask_ &
        ~static_cast<std::uint32_t>(setChannels));
    platform::writePortLines(
        pinmap::kGateChannelPins[0], gateLinesFor(rising), gateLinesFor(falling));
    channelHighMask_ = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(channelHighMask_) | rising) &
        ~static_cast<std::uint32_t>(falling));
}

bool GateOutputDriver::channelStateHigh(const std::size_t channelIndex) const {
    return channelIndex < kChannelCount && (channelHighMask_ & channelBit(channelIndex)) != 0U;
}

bool GateOutputDriver::outputStageEnabled() const {
    return outputStageEnabled_;
}

}  // namespace clockfw::hal
