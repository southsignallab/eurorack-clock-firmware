/**
 * @file clock_engine_timing.cpp
 * @brief Timing-rate calculations and scheduler-resolution limiting for ClockEngine.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "engine/clock_engine.h"

#include <algorithm>

#include "clock_core.h"
#include "config.h"
#include "domain/groove_catalog.h"
#include "domain/custom_groove.h"

namespace clockfw::engine {

std::uint32_t ClockEngine::effectiveBpmMilli() const {
    return effectiveBpmMilli(*configuration_, externalLocked_, externalBpmMilli_);
}

std::uint32_t ClockEngine::effectiveBpmMilli(
    const EngineConfiguration& configuration,
    const bool externalLocked,
    const std::uint32_t externalBpmMilli) {
    std::uint32_t bpmMilli = static_cast<std::uint32_t>(configuration.bpm) * 1000U;
    if (configuration.source == ClockSource::External) {
        if ((externalLocked || configuration.syncLossMode == SyncLossMode::Freewheel) &&
            externalBpmMilli != 0U) {
            bpmMilli = externalBpmMilli;
        }
    } else if (configuration.source == ClockSource::Auto) {
        if ((externalLocked || configuration.syncLossMode == SyncLossMode::Freewheel) &&
            externalBpmMilli != 0U) {
            bpmMilli = externalBpmMilli;
        }
    }
    return bpmMilli;
}

ClockEngine::ScheduleInputs ClockEngine::makeScheduleInputs(
    const EngineConfiguration& configuration,
    const std::uint32_t* const customGrooveSpacingQ16,
    const bool externalLocked,
    const std::uint32_t externalBpmMilli) {
    ScheduleInputs inputs{};
    inputs.configuration = &configuration;
    inputs.customGrooveSpacingQ16 = customGrooveSpacingQ16;
    inputs.bpmMilli = effectiveBpmMilli(configuration, externalLocked, externalBpmMilli);
    inputs.minimumIntervalQ32 = minimumOutputIntervalQ32(inputs.bpmMilli, configuration.beatUnit);
    return inputs;
}

ClockEngine::ScheduleInputs ClockEngine::liveScheduleInputs() const {
    return makeScheduleInputs(
        *configuration_, customGrooveSpacingQ16_, externalLocked_, externalBpmMilli_);
}

std::uint64_t ClockEngine::minimumOutputIntervalQ32() const {
    return minimumOutputIntervalQ32(effectiveBpmMilli(), configuration_->beatUnit);
}

std::uint64_t ClockEngine::minimumOutputIntervalQ32(
    const std::uint32_t bpmMilli,
    std::uint8_t beatUnit) {
    if (bpmMilli == 0U) {
        return 1U;
    }
    beatUnit = beatUnit != 0U ? beatUnit : 4U;
    const std::uint64_t numerator =
        static_cast<std::uint64_t>(bpmMilli) * beatUnit * core::kQ32One;
    const std::uint64_t denominator =
        60ULL * 4ULL * config::kSchedulerFrequencyHz * 1000ULL;
    return (numerator + denominator - 1ULL) / denominator;
}

void ClockEngine::resolveChannelRate(
    const EngineConfiguration& configuration,
    const std::size_t channelIndex,
    std::uint32_t& numerator,
    std::uint32_t& denominator) {
    const ChannelConfig& channel = configuration.channels[channelIndex];
    numerator = 1U;
    denominator = 1U;

    if (channel.common.mode == ChannelMode::Clock) {
        core::calculateEffectiveClockRate(
            channel.common,
            channel.clock.meter.unit,
            configuration.beatUnit,
            numerator,
            denominator);
    } else if (channel.common.mode == ChannelMode::Euclid ||
               channel.common.mode == ChannelMode::Sequencer) {
        core::calculateEffectivePatternRate(
            channel.common,
            configuration.beatUnit,
            numerator,
            denominator);
    }
}

std::uint64_t ClockEngine::calculateNominalBaseIntervalQ32(
    const std::size_t channelIndex) const {
    return calculateNominalBaseIntervalQ32(liveScheduleInputs(), channelIndex);
}

std::uint64_t ClockEngine::calculateNominalBaseIntervalQ32(
    const ScheduleInputs& inputs,
    const std::size_t channelIndex) {
    std::uint32_t numerator = 1U;
    std::uint32_t denominator = 1U;
    resolveChannelRate(*inputs.configuration, channelIndex, numerator, denominator);

    volatile std::uint32_t ignoredRemainder = 0U;
    const std::uint64_t interval =
        core::calculateNextIntervalQ32(numerator, denominator, ignoredRemainder);
    return std::max(interval, inputs.minimumIntervalQ32);
}

std::uint64_t ClockEngine::calculateBaseEventOffsetQ32(
    const ScheduleInputs& inputs,
    const std::size_t channelIndex,
    const std::uint64_t eventSerial) {
    if (eventSerial == 0U) {
        return 0U;
    }

    std::uint32_t numerator = 1U;
    std::uint32_t denominator = 1U;
    resolveChannelRate(*inputs.configuration, channelIndex, numerator, denominator);
    if (numerator == 0U) {
        numerator = 1U;
    }

    const std::uint64_t scaledInterval =
        static_cast<std::uint64_t>(denominator) * core::kQ32One;
    const std::uint64_t quotient = scaledInterval / numerator;
    const std::uint64_t remainder = scaledInterval % numerator;
    const std::uint64_t minimumInterval = inputs.minimumIntervalQ32;

    // Once the requested rate exceeds the scheduler's physical edge resolution,
    // the effective grid is deliberately clamped to one scheduler interval.
    if (quotient < minimumInterval) {
        if (eventSerial > UINT64_MAX / minimumInterval) {
            return UINT64_MAX;
        }
        return eventSerial * minimumInterval;
    }

    // floor(serial * scaledInterval / numerator), evaluated without requiring
    // a 128-bit multiply. Splitting serial into full numerator-sized groups
    // keeps every intermediate bounded by the final representable result.
    if (quotient != 0U && eventSerial > UINT64_MAX / quotient) {
        return UINT64_MAX;
    }
    const std::uint64_t whole = eventSerial * quotient;
    const std::uint64_t groups = eventSerial / numerator;
    const std::uint64_t tail = eventSerial % numerator;
    if (remainder != 0U && groups > (UINT64_MAX - whole) / remainder) {
        return UINT64_MAX;
    }
    const std::uint64_t groupedRemainder = groups * remainder;
    const std::uint64_t tailRemainder = (tail * remainder) / numerator;
    if (tailRemainder > UINT64_MAX - whole - groupedRemainder) {
        return UINT64_MAX;
    }
    return whole + groupedRemainder + tailRemainder;
}

std::uint64_t ClockEngine::calculateEventPositionQ32(
    const std::size_t channelIndex,
    const std::uint64_t eventSerial,
    std::uint64_t* const nominalSlotQ32) const {
    return calculateEventPositionQ32(
        liveScheduleInputs(),
        channelIndex,
        channelRuntime_[channelIndex].scheduleEpochQ32,
        eventSerial,
        nominalSlotQ32);
}

std::uint64_t ClockEngine::calculateEventPositionQ32(
    const ScheduleInputs& inputs,
    const std::size_t channelIndex,
    const std::uint64_t scheduleEpochQ32,
    const std::uint64_t eventSerial,
    std::uint64_t* const nominalSlotQ32) const {
    const std::uint64_t baseIntervalQ32 = calculateNominalBaseIntervalQ32(inputs, channelIndex);
    const std::uint64_t phaseOffsetQ32 = core::calculatePhaseOffsetQ32(
        baseIntervalQ32,
        inputs.configuration->channels[channelIndex].common.phasePercent);
    const std::uint64_t baseOffsetQ32 =
        calculateBaseEventOffsetQ32(inputs, channelIndex, eventSerial);
    if (nominalSlotQ32 != nullptr) {
        // Grid slot without phase, swing, groove, or humanize (F-063 identity of an event).
        *nominalSlotQ32 = baseOffsetQ32 > UINT64_MAX - scheduleEpochQ32
            ? UINT64_MAX
            : scheduleEpochQ32 + baseOffsetQ32;
    }
    const std::uint64_t swingOffsetQ32 = (eventSerial & 1ULL) != 0ULL
        ? calculateSwingDeltaQ32(inputs, channelIndex, baseIntervalQ32)
        : 0U;
    const GrooveSettings& grooveSettings =
        inputs.configuration->channels[channelIndex].common.groove;
    const std::uint64_t minimumIntervalQ32 = inputs.minimumIntervalQ32;

    if (scheduleEpochQ32 > UINT64_MAX - phaseOffsetQ32) {
        return UINT64_MAX;
    }
    const std::uint64_t phasePositionQ32 = scheduleEpochQ32 + phaseOffsetQ32;
    if (baseOffsetQ32 > UINT64_MAX - phasePositionQ32) {
        return UINT64_MAX;
    }
    const std::uint64_t nominalPositionQ32 = phasePositionQ32 + baseOffsetQ32;

    std::uint64_t shapedPositionQ32 = nominalPositionQ32;
    const bool previewActive = customGroovePreviewActive_[channelIndex];
    if (previewActive ||
        (grooveSettings.preset == GroovePreset::Custom &&
         grooveSettings.customSlot < kCustomGrooveSlotCount)) {
        const CustomGroovePattern& customPattern = previewActive
            ? customGroovePreviews_[channelIndex]
            : customGrooves_[grooveSettings.customSlot];
        const std::uint8_t customAmount = previewActive
            ? customGroovePreviewAmount_[channelIndex]
            : grooveSettings.amountPercent;
        const std::uint8_t customRotation = previewActive
            ? customGroovePreviewRotation_[channelIndex]
            : grooveSettings.rotation;
        const std::int16_t offset256 = customGrooveOffset256(
            customPattern,
            eventSerial,
            customAmount,
            customRotation);
        const std::int64_t requestedCustomQ32 =
            static_cast<std::int64_t>((baseIntervalQ32 * static_cast<std::uint64_t>(
                offset256 < 0 ? -offset256 : offset256)) / 256ULL) *
            (offset256 < 0 ? -1LL : 1LL);
        const std::uint64_t symmetricLimitQ32 = baseIntervalQ32 > minimumIntervalQ32
            ? (baseIntervalQ32 - minimumIntervalQ32) / 2ULL
            : 0U;
        const std::int64_t combinedQ32 = static_cast<std::int64_t>(swingOffsetQ32) +
            requestedCustomQ32;
        const std::int64_t boundedCombinedQ32 = std::clamp<std::int64_t>(
            combinedQ32,
            -static_cast<std::int64_t>(symmetricLimitQ32),
            static_cast<std::int64_t>(symmetricLimitQ32));
        if (boundedCombinedQ32 >= 0) {
            const std::uint64_t delayQ32 = static_cast<std::uint64_t>(boundedCombinedQ32);
            shapedPositionQ32 = delayQ32 > UINT64_MAX - nominalPositionQ32
                ? UINT64_MAX
                : nominalPositionQ32 + delayQ32;
        } else {
            const std::uint64_t advanceQ32 = static_cast<std::uint64_t>(-boundedCombinedQ32);
            shapedPositionQ32 = advanceQ32 >= nominalPositionQ32 - phasePositionQ32
                ? phasePositionQ32
                : nominalPositionQ32 - advanceQ32;
        }
    } else {
        const std::uint16_t groovePermille = groove::delayPermille(
            grooveSettings.preset,
            eventSerial,
            grooveSettings.amountPercent,
            grooveSettings.rotation);
        const std::uint64_t requestedGrooveQ32 =
            (baseIntervalQ32 * groovePermille) / 1000ULL;
        const std::uint64_t maximumCombinedDelayQ32 = baseIntervalQ32 > minimumIntervalQ32
            ? baseIntervalQ32 - minimumIntervalQ32
            : 0U;
        const std::uint64_t grooveOffsetQ32 = swingOffsetQ32 >= maximumCombinedDelayQ32
            ? 0U
            : std::min(requestedGrooveQ32, maximumCombinedDelayQ32 - swingOffsetQ32);
        if (swingOffsetQ32 > UINT64_MAX - nominalPositionQ32 ||
            grooveOffsetQ32 > UINT64_MAX - nominalPositionQ32 - swingOffsetQ32) {
            return UINT64_MAX;
        }
        shapedPositionQ32 = nominalPositionQ32 + swingOffsetQ32 + grooveOffsetQ32;
    }
    const std::int64_t humanizeOffsetQ32 =
        calculateHumanizeOffsetQ32(inputs, channelIndex, eventSerial);
    if (humanizeOffsetQ32 >= 0) {
        const std::uint64_t delayQ32 = static_cast<std::uint64_t>(humanizeOffsetQ32);
        return delayQ32 > UINT64_MAX - shapedPositionQ32
            ? UINT64_MAX
            : shapedPositionQ32 + delayQ32;
    }

    const std::uint64_t advanceQ32 = static_cast<std::uint64_t>(-humanizeOffsetQ32);
    const std::uint64_t earliestPositionQ32 = phasePositionQ32;
    return advanceQ32 >= shapedPositionQ32 - earliestPositionQ32
        ? earliestPositionQ32
        : shapedPositionQ32 - advanceQ32;
}

std::uint64_t ClockEngine::calculateBaseIntervalUs(
    const ScheduleInputs& inputs,
    const std::size_t channelIndex) {
    std::uint32_t numerator = 1U;
    std::uint32_t denominator = 1U;
    resolveChannelRate(*inputs.configuration, channelIndex, numerator, denominator);

    std::uint32_t bpmMilli = inputs.bpmMilli;
    if (bpmMilli == 0U) {
        bpmMilli = 120000U;
    }
    const std::uint8_t beatUnit =
        inputs.configuration->beatUnit != 0U ? inputs.configuration->beatUnit : 4U;
    const std::uint64_t divisor =
        static_cast<std::uint64_t>(bpmMilli) * beatUnit * numerator;
    if (divisor == 0U) {
        return config::kSchedulerTickUs;
    }
    const std::uint64_t intervalUs =
        (60000000000ULL * 4ULL * denominator) / divisor;
    return std::max<std::uint64_t>(intervalUs, config::kSchedulerTickUs);
}

std::uint32_t ClockEngine::calculateCustomGrooveSpacingQ16(
    const std::size_t channelIndex,
    const ChannelConfig& channel,
    const bool previewActive) const {
    // Same pattern selection as calculateEventPositionQ32(). Foreground only: the
    // library, previews and channel configuration have no writer in interrupt context.
    if (previewActive) {
        return customGrooveMinimumSpacingQ16(
            customGroovePreviews_[channelIndex],
            customGroovePreviewAmount_[channelIndex],
            customGroovePreviewRotation_[channelIndex],
            channel.common.swingPercent);
    }
    const GrooveSettings& grooveSettings = channel.common.groove;
    if (grooveSettings.preset == GroovePreset::Custom &&
        grooveSettings.customSlot < kCustomGrooveSlotCount) {
        return customGrooveMinimumSpacingQ16(
            customGrooves_[grooveSettings.customSlot],
            grooveSettings.amountPercent,
            grooveSettings.rotation,
            channel.common.swingPercent);
    }
    return kCustomGrooveSpacingQ16One;
}

std::uint16_t ClockEngine::effectiveHumanizeUs(
    const ScheduleInputs& inputs,
    const std::size_t channelIndex) const {
    const EngineConfiguration& configuration = *inputs.configuration;
    if (configuration.operatingMode != OperatingMode::UnifiedClock ||
        configuration.unifiedHumanizeUs == 0U || channelIndex >= kChannelCount) {
        return 0U;
    }

    const ChannelConfig& channel = configuration.channels[channelIndex];
    const std::uint64_t baseIntervalUs = calculateBaseIntervalUs(inputs, channelIndex);
    std::uint64_t shortestShapedIntervalUs = baseIntervalUs;
    const GrooveSettings& grooveSettings = channel.common.groove;
    if (customGroovePreviewActive_[channelIndex] ||
        (grooveSettings.preset == GroovePreset::Custom &&
         grooveSettings.customSlot < kCustomGrooveSlotCount)) {
        // Custom offsets are signed, so the factory maximum-delay bound does not
        // apply. The foreground-cached spacing is a Q16 fraction of the interval:
        // one multiply and one shift, no division in scheduler context.
        const std::uint64_t spacingQ16 = inputs.customGrooveSpacingQ16[channelIndex];
        shortestShapedIntervalUs = baseIntervalUs <= UINT64_MAX / (2ULL * kCustomGrooveSpacingQ16One)
            ? (baseIntervalUs * spacingQ16) >> 16U
            : (baseIntervalUs >> 16U) * spacingQ16;
    } else {
        const std::uint8_t swingPercent = std::min<std::uint8_t>(channel.common.swingPercent, 50U);
        const std::uint64_t swingDelayUs = (baseIntervalUs * swingPercent) / 100ULL;
        const std::uint16_t groovePermille = groove::maximumDelayPermille(
            grooveSettings.preset,
            grooveSettings.amountPercent);
        const std::uint64_t grooveDelayUs = (baseIntervalUs * groovePermille) / 1000ULL;
        const std::uint64_t totalDelayUs = std::min<std::uint64_t>(
            swingDelayUs + grooveDelayUs,
            baseIntervalUs > config::kSchedulerTickUs
                ? baseIntervalUs - config::kSchedulerTickUs
                : 0U);
        shortestShapedIntervalUs = baseIntervalUs - totalDelayUs;
    }
    if (shortestShapedIntervalUs <= config::kSchedulerTickUs) {
        return 0U;
    }

    // Adjacent events may receive opposite signed humanize offsets. Reserve one
    // scheduler quantum even after deterministic Swing + Groove displacement.
    const std::uint64_t maximumSafeUs =
        (shortestShapedIntervalUs - config::kSchedulerTickUs) / 2ULL;
    const std::uint64_t quantizedSafeUs =
        (maximumSafeUs / config::kSchedulerTickUs) * config::kSchedulerTickUs;
    const std::uint64_t configuredUs = std::min<std::uint64_t>(
        configuration.unifiedHumanizeUs, config::kMaximumHumanizeUs);
    return static_cast<std::uint16_t>(std::min(configuredUs, quantizedSafeUs));
}

std::int64_t ClockEngine::calculateHumanizeOffsetQ32(
    const ScheduleInputs& inputs,
    const std::size_t channelIndex,
    const std::uint64_t eventSerial) const {
    // Keep the shared restart/downbeat exact. Humanization begins with the next edge.
    if (eventSerial == 0U) {
        return 0;
    }
    const std::uint16_t humanizeUs = effectiveHumanizeUs(inputs, channelIndex);
    const std::uint32_t maximumTicks = humanizeUs / config::kSchedulerTickUs;
    if (maximumTicks == 0U) {
        return 0;
    }

    // Stateless hash: rescheduling the same event reproduces exactly the same
    // per-output offset, so live UI edits cannot advance a hidden RNG stream.
    std::uint32_t hash = static_cast<std::uint32_t>(eventSerial) ^
        static_cast<std::uint32_t>(eventSerial >> 32U) ^
        (static_cast<std::uint32_t>(channelIndex + 1U) * 0x9E3779B9U);
    hash ^= hash >> 16U;
    hash *= 0x7FEB352DU;
    hash ^= hash >> 15U;
    hash *= 0x846CA68BU;
    hash ^= hash >> 16U;

    const std::uint32_t span = maximumTicks * 2U + 1U;
    const std::int32_t signedTicks = static_cast<std::int32_t>(hash % span) -
        static_cast<std::int32_t>(maximumTicks);
    const std::int64_t tickQ32 = static_cast<std::int64_t>(inputs.minimumIntervalQ32);
    return static_cast<std::int64_t>(signedTicks) * tickQ32;
}

std::uint64_t ClockEngine::calculateSwingDeltaQ32(
    const ScheduleInputs& inputs,
    const std::size_t channelIndex,
    const std::uint64_t baseIntervalQ32) {
    const std::uint64_t minimumIntervalQ32 = inputs.minimumIntervalQ32;
    std::uint8_t swingPercent = inputs.configuration->channels[channelIndex].common.swingPercent;
    if (swingPercent > 50U) {
        swingPercent = 50U;
    }
    if (swingPercent == 0U || baseIntervalQ32 <= minimumIntervalQ32) {
        return 0U;
    }
    const std::uint64_t requestedDelta = (baseIntervalQ32 * swingPercent) / 100ULL;
    return std::min(requestedDelta, baseIntervalQ32 - minimumIntervalQ32);
}

}  // namespace clockfw::engine
