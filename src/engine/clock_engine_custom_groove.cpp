/**
 * @file clock_engine_custom_groove.cpp
 * @brief Custom Groove library and runtime-preview updates for ClockEngine.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "engine/clock_engine_custom_groove.h"

#include "engine/output_mode_resolver.h"
#include "hal/interrupt_lock.h"

namespace clockfw::engine {

void ClockEngine::calculateCustomGrooveSpacingsQ16(
    const ClockState& state,
    std::uint32_t (&spacingQ16)[kChannelCount]) const {
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        spacingQ16[channelIndex] = calculateCustomGrooveSpacingQ16(
            channelIndex,
            resolvePhysicalOutputConfiguration(state, channelIndex),
            customGroovePreviewActive_[channelIndex]);
    }
}

void ClockEngine::installCustomGrooveSpacingsUnsafe(
    const std::uint32_t (&spacingQ16)[kChannelCount]) {
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        customGrooveSpacingQ16_[channelIndex] = spacingQ16[channelIndex];
    }
}

void ClockEngine::updateCustomGrooveSlot(
    const std::uint8_t slotIndex,
    const CustomGroovePattern& pattern,
    const bool rescheduleAffected) {
    if (slotIndex >= kCustomGrooveSlotCount || !isCustomGroovePatternValid(pattern)) {
        return;
    }

    // Foreground: compute the Humanize spacing bound before entering the critical section.
    std::uint32_t spacingQ16[kChannelCount]{};
    bool usesSlot[kChannelCount]{};
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        const ChannelConfig& channel = configuration_.channels[channelIndex];
        const GrooveSettings& grooveSettings = channel.common.groove;
        usesSlot[channelIndex] = grooveSettings.preset == GroovePreset::Custom &&
            grooveSettings.customSlot == slotIndex;
        if (usesSlot[channelIndex] && !customGroovePreviewActive_[channelIndex]) {
            spacingQ16[channelIndex] = customGrooveMinimumSpacingQ16(
                pattern, grooveSettings.amountPercent, grooveSettings.rotation,
                channel.common.swingPercent);
        }
    }

    hal::InterruptLock interruptLock;
    customGrooves_[slotIndex] = pattern;
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        if (usesSlot[channelIndex] && !customGroovePreviewActive_[channelIndex]) {
            customGrooveSpacingQ16_[channelIndex] = spacingQ16[channelIndex];
        }
    }
    if (!rescheduleAffected) {
        return;
    }
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        if (usesSlot[channelIndex]) {
            scheduleChannelFromCurrentPosition(channelIndex);
        }
    }
}

void ClockEngine::setCustomGroovePreview(
    const std::size_t channelIndex,
    const CustomGroovePattern& pattern,
    const std::uint8_t amountPercent,
    const std::uint8_t rotation,
    const bool rescheduleChannel) {
    if (channelIndex >= kChannelCount || !isCustomGroovePatternValid(pattern) ||
        amountPercent > 100U || rotation >= pattern.length) {
        return;
    }
    const std::uint32_t spacingQ16 = customGrooveMinimumSpacingQ16(
        pattern, amountPercent, rotation, configuration_.channels[channelIndex].common.swingPercent);
    hal::InterruptLock interruptLock;
    customGroovePreviews_[channelIndex] = pattern;
    customGroovePreviewAmount_[channelIndex] = amountPercent;
    customGroovePreviewRotation_[channelIndex] = rotation;
    customGroovePreviewActive_[channelIndex] = true;
    customGrooveSpacingQ16_[channelIndex] = spacingQ16;
    if (rescheduleChannel) {
        scheduleChannelFromCurrentPosition(channelIndex);
    }
}

void ClockEngine::clearCustomGroovePreview(
    const std::size_t channelIndex,
    const bool rescheduleChannel) {
    if (channelIndex >= kChannelCount) {
        return;
    }
    const std::uint32_t spacingQ16 = calculateCustomGrooveSpacingQ16(
        channelIndex, configuration_.channels[channelIndex], false);
    hal::InterruptLock interruptLock;
    customGroovePreviewActive_[channelIndex] = false;
    customGrooveSpacingQ16_[channelIndex] = spacingQ16;
    if (rescheduleChannel) {
        scheduleChannelFromCurrentPosition(channelIndex);
    }
}


}  // namespace clockfw::engine
