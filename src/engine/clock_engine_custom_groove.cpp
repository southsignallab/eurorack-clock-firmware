/**
 * @file clock_engine_custom_groove.cpp
 * @brief Custom Groove library and runtime-preview updates for ClockEngine.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "engine/clock_engine_custom_groove.h"

#include "engine/output_mode_resolver.h"

namespace clockfw::engine {

void ClockEngine::calculateCustomGrooveSpacingsQ16(
    const ClockState& state,
    std::uint32_t (&spacingQ16)[kChannelCount]) const {
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        spacingQ16[channelIndex] = calculateCustomGrooveSpacingQ16(
            *grooveSources_, channelIndex, resolvePhysicalOutputConfiguration(state, channelIndex));
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

    // Foreground: the pattern goes into the spare buffer, the scheduler keeps
    // reading the old one until the swap exchanges the table pointer (F-072).
    GrooveSources* const grooves = stageGrooveSources();
    *spareCustomGroove_ = pattern;
    StagedConfiguration staged = stageLiveConfiguration(grooves);
    staged.releasedGroove = grooves->slots[slotIndex];
    grooves->slots[slotIndex] = spareCustomGroove_;
    std::uint8_t affected = 0U;
    for (std::size_t channelIndex = 0U; channelIndex < kChannelCount; ++channelIndex) {
        const ChannelConfig& channel = configuration_->channels[channelIndex];
        const GrooveSettings& grooveSettings = channel.common.groove;
        if (grooveSettings.preset != GroovePreset::Custom ||
            grooveSettings.customSlot != slotIndex) {
            continue;
        }
        affected = static_cast<std::uint8_t>(affected | (1U << channelIndex));
        staged.customGrooveSpacingQ16[channelIndex] =
            calculateCustomGrooveSpacingQ16(*grooves, channelIndex, channel);
    }
    swapConfiguration(staged, rescheduleAffected ? affected : std::uint8_t{0U}, rescheduleAffected);
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
    GrooveSources* const grooves = stageGrooveSources();
    *spareCustomGroove_ = pattern;
    StagedConfiguration staged = stageLiveConfiguration(grooves);
    staged.releasedGroove = grooves->previews[channelIndex];
    grooves->previews[channelIndex] = spareCustomGroove_;
    grooves->previewAmount[channelIndex] = amountPercent;
    grooves->previewRotation[channelIndex] = rotation;
    grooves->previewActive[channelIndex] = true;
    staged.customGrooveSpacingQ16[channelIndex] = calculateCustomGrooveSpacingQ16(
        *grooves, channelIndex, configuration_->channels[channelIndex]);
    swapConfiguration(
        staged,
        static_cast<std::uint8_t>(rescheduleChannel ? (1U << channelIndex) : 0U),
        rescheduleChannel);
}

void ClockEngine::clearCustomGroovePreview(
    const std::size_t channelIndex,
    const bool rescheduleChannel) {
    if (channelIndex >= kChannelCount) {
        return;
    }
    // The preview buffer stays where it is; only the table no longer uses it.
    GrooveSources* const grooves = stageGrooveSources();
    StagedConfiguration staged = stageLiveConfiguration(grooves);
    grooves->previewActive[channelIndex] = false;
    staged.customGrooveSpacingQ16[channelIndex] = calculateCustomGrooveSpacingQ16(
        *grooves, channelIndex, configuration_->channels[channelIndex]);
    swapConfiguration(
        staged,
        static_cast<std::uint8_t>(rescheduleChannel ? (1U << channelIndex) : 0U),
        rescheduleChannel);
}

}  // namespace clockfw::engine
