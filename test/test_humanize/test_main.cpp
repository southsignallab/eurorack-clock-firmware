/**
 * @file test_main.cpp
 * @brief Atomic native tests for deterministic One Clock humanize timing.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <unity.h>
#include <Arduino.h>

#include "config.h"
#include "domain/clock_types.h"
#include "domain/default_configuration.h"
#include "engine/clock_engine.h"
#include "hal/gate_output_driver.h"
#include "pin_map.h"

using namespace clockfw;
void setUp(){fakefw::resetArduino();} void tearDown(){}

namespace {
using EdgeArray=std::array<std::uint32_t,kChannelCount>;
ClockState makeUnified(std::uint16_t humanize,std::uint8_t swing=0U){ClockState s{};initializeFactoryDefaults(s);s.bpm=120U;s.operatingMode=OperatingMode::UnifiedClock;s.source=ClockSource::Internal;s.unifiedClock.rate={ClockRatioMode::Multiply,1U,1U,1U};s.unifiedClock.swingPercent=swing;s.unifiedClock.phasePercent=0U;s.unifiedClock.gateLengthMs=1U;s.unifiedClock.humanizeUs=humanize;return s;}
EdgeArray collectSecondEdges(const ClockState& state){hal::GateOutputDriver gates;gates.beginDisabled();gates.enableOutputStage();engine::ClockEngine engine(gates);engine.begin(state);engine.play();fakefw::writes.clear();engine.processSchedulerTick();EdgeArray out{};out.fill(0xFFFFFFFFU);for(std::uint32_t tick=1U;tick<20000U;++tick){fakefw::writes.clear();engine.processSchedulerTick();for(std::size_t i=0;i<kChannelCount;++i){if(out[i]!=0xFFFFFFFFU)continue;const bool rose=std::any_of(fakefw::writes.begin(),fakefw::writes.end(),[i](const fakefw::PinWrite&w){return w.pin==pinmap::kGateChannelPins[i]&&w.value==HIGH;});if(rose)out[i]=tick;}if(std::all_of(out.begin(),out.end(),[](auto v){return v!=0xFFFFFFFFU;}))break;}return out;}
void assertAllAt(const EdgeArray&a,std::uint32_t expected){for(auto v:a)TEST_ASSERT_EQUAL_UINT32(expected,v);}
void assertWithin(const EdgeArray&a,std::uint32_t low,std::uint32_t high){for(auto v:a)TEST_ASSERT_TRUE(v>=low&&v<=high);}
void testHumanizeOffKeepsAllOutputsCoincident(){assertAllAt(collectSecondEdges(makeUnified(0U)),9999U);}
void testHumanize250StaysWithinConfiguredWindow(){assertWithin(collectSecondEdges(makeUnified(250U)),9994U,10005U);}
void testHumanize500StaysWithinConfiguredWindow(){assertWithin(collectSecondEdges(makeUnified(500U)),9989U,10010U);}
void testHumanize1000StaysWithinConfiguredWindow(){assertWithin(collectSecondEdges(makeUnified(1000U)),9979U,10020U);}
void testHumanize2000StaysWithinConfiguredWindow(){assertWithin(collectSecondEdges(makeUnified(2000U)),9959U,10040U);}
void testHumanizeProducesChannelSpread(){const auto a=collectSecondEdges(makeUnified(2000U));const auto [mn,mx]=std::minmax_element(a.begin(),a.end());TEST_ASSERT_TRUE(*mx>*mn);}
void testHumanizeIsDeterministicAcrossFreshRuns(){TEST_ASSERT_TRUE(collectSecondEdges(makeUnified(2000U))==collectSecondEdges(makeUnified(2000U)));}
void testHumanizeNeverAdvancesBeforePreviousEvent(){const auto a=collectSecondEdges(makeUnified(2000U));for(auto v:a)TEST_ASSERT_TRUE(v>0U);}
void testHumanizeWithFiftyPercentSwingStillMaintainsOrder(){const auto a=collectSecondEdges(makeUnified(2000U,50U));for(auto v:a)TEST_ASSERT_TRUE(v>10000U);}
void testHumanizeStoredOutsideUnifiedModeHasNoEffect(){auto s=makeUnified(2000U);s.operatingMode=OperatingMode::Independent;for(auto&c:s.channels){c.common.mode=ChannelMode::Clock;c.common.probabilityPercent=100U;c.common.swingPercent=0U;c.common.phasePercent=0U;c.common.gateLengthMs=1U;c.common.rate={ClockRatioMode::Multiply,1U,1U,1U};c.clock.meter={4U,4U};}assertAllAt(collectSecondEdges(s),9999U);}
void testHumanizeAboveConfiguredMaximumIsSafelyBounded(){auto s=makeUnified(65535U);const auto a=collectSecondEdges(s);const std::uint32_t maxTicks=config::kMaximumHumanizeUs/config::kSchedulerTickUs;assertWithin(a,9999U-maxTicks,10000U+maxTicks);}
void testHumanizeQuantizationIsSchedulerAligned(){const auto a=collectSecondEdges(makeUnified(2000U));for(auto v:a)TEST_ASSERT_TRUE(v>=9959U&&v<=10040U);}

struct RiseLog {
    std::array<std::uint32_t, kChannelCount> rises{};
    std::array<std::uint32_t, kChannelCount> missingLowGaps{};
};

/** Counts physical rising edges and rises without an intervening LOW on every output. */
RiseLog collectCustomGrooveRises(
    const CustomGroovePattern& pattern,
    const std::uint16_t humanizeUs,
    const std::uint8_t swingPercent,
    const std::uint8_t rotation,
    const std::uint32_t ticks) {
    ClockState state = makeUnified(humanizeUs, swingPercent);
    state.bpm = 240U;
    state.unifiedClock.rate = {ClockRatioMode::Multiply, 4U, 1U, 1U};
    state.unifiedClock.groove = {GroovePreset::Custom, 100U, rotation, 0U};
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    gates.enableOutputStage();
    engine::ClockEngine engine(gates);
    engine.updateCustomGrooveSlot(0U, pattern, false);
    engine.begin(state);
    engine.play();
    RiseLog log{};
    std::array<bool, kChannelCount> high{};
    for (std::uint32_t tick = 0U; tick < ticks; ++tick) {
        fakefw::writes.clear();
        engine.processSchedulerTick();
        for (const auto& write : fakefw::writes) {
            for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
                if (write.pin != pinmap::kGateChannelPins[channel]) {
                    continue;
                }
                if (write.value == HIGH) {
                    if (high[channel]) {
                        ++log.missingLowGaps[channel];
                    }
                    ++log.rises[channel];
                }
                high[channel] = write.value == HIGH;
            }
        }
    }
    return log;
}

/**
 * @brief Same defect class as #6: Humanize must respect the Custom Groove event spacing.
 *
 * 240 BPM x4 gives a 62.5 ms grid. Swing 50 plus a fully opposed Custom Groove leaves
 * about 2 ms between an odd event and the following even event, so +-2 ms Humanize
 * would reorder or merge those events. Every output must still produce one rising
 * edge per straight reference event.
 */
void testHumanizeRespectsCustomGrooveSpacing() {
    CustomGroovePattern pattern{};
    pattern.length = 2U;
    pattern.offsets256[0] = -120;
    pattern.offsets256[1] = 120;
    // 64 grid intervals end on a nominal grid point that no shaped event occupies.
    constexpr std::uint32_t kTicks = 64U * 1250U;
    for (const std::uint8_t rotation : std::array<std::uint8_t, 2U>{{0U, 1U}}) {
        for (const std::uint8_t swing : std::array<std::uint8_t, 2U>{{0U, 50U}}) {
            const RiseLog reference = collectCustomGrooveRises(pattern, 0U, swing, rotation, kTicks);
            const RiseLog humanized =
                collectCustomGrooveRises(pattern, config::kMaximumHumanizeUs, swing, rotation, kTicks);
            for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
                TEST_ASSERT_TRUE(reference.rises[channel] >= 60U);
                TEST_ASSERT_EQUAL_UINT32(0U, reference.missingLowGaps[channel]);
                TEST_ASSERT_EQUAL_UINT32(reference.rises[channel], humanized.rises[channel]);
                TEST_ASSERT_EQUAL_UINT32(0U, humanized.missingLowGaps[channel]);
            }
        }
    }
}

/** @brief Humanize keeps its full range when the Custom Groove leaves enough spacing. */
void testHumanizeKeepsFullRangeForWideCustomGrooveSpacing() {
    CustomGroovePattern pattern{};
    pattern.length = 2U;
    pattern.offsets256[0] = 0;
    pattern.offsets256[1] = 20;
    ClockState state = makeUnified(config::kMaximumHumanizeUs);
    state.unifiedClock.groove = {GroovePreset::Custom, 100U, 0U, 0U};
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    gates.enableOutputStage();
    engine::ClockEngine engine(gates);
    engine.updateCustomGrooveSlot(0U, pattern, false);
    engine.begin(state);
    engine.play();
    EdgeArray second{};
    second.fill(0xFFFFFFFFU);
    engine.processSchedulerTick();
    for (std::uint32_t tick = 1U; tick < 20000U; ++tick) {
        fakefw::writes.clear();
        engine.processSchedulerTick();
        for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
            if (second[channel] != 0xFFFFFFFFU) {
                continue;
            }
            for (const auto& write : fakefw::writes) {
                if (write.pin == pinmap::kGateChannelPins[channel] && write.value == HIGH) {
                    second[channel] = tick;
                }
            }
        }
    }
    // Event 1 sits at 10000 + 20/256 of 10000 ticks = 10781 ticks; Humanize may move it
    // by up to 40 ticks, and the four outputs must not all collapse onto one tick.
    const auto [low, high] = std::minmax_element(second.begin(), second.end());
    TEST_ASSERT_TRUE(*low >= 10780U - 40U);
    TEST_ASSERT_TRUE(*high <= 10781U + 40U);
    TEST_ASSERT_TRUE(*high > *low);
}
}
int main(){UNITY_BEGIN();RUN_TEST(testHumanizeOffKeepsAllOutputsCoincident);RUN_TEST(testHumanize250StaysWithinConfiguredWindow);RUN_TEST(testHumanize500StaysWithinConfiguredWindow);RUN_TEST(testHumanize1000StaysWithinConfiguredWindow);RUN_TEST(testHumanize2000StaysWithinConfiguredWindow);RUN_TEST(testHumanizeProducesChannelSpread);RUN_TEST(testHumanizeIsDeterministicAcrossFreshRuns);RUN_TEST(testHumanizeNeverAdvancesBeforePreviousEvent);RUN_TEST(testHumanizeWithFiftyPercentSwingStillMaintainsOrder);RUN_TEST(testHumanizeStoredOutsideUnifiedModeHasNoEffect);RUN_TEST(testHumanizeAboveConfiguredMaximumIsSafelyBounded);RUN_TEST(testHumanizeQuantizationIsSchedulerAligned);RUN_TEST(testHumanizeRespectsCustomGrooveSpacing);RUN_TEST(testHumanizeKeepsFullRangeForWideCustomGrooveSpacing);return UNITY_END();}
