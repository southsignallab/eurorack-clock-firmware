/**
 * @file test_main.cpp
 * @brief Atomic native behavioral tests for the tap-tempo estimator.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include <cstdint>
#include <limits>
#include <unity.h>

#include "config.h"
#include "services/tap_tempo.h"

using clockfw::services::TapTempo;

namespace {
/** Converts a millisecond tap time into the estimator's microsecond time base (wraps mod 2^32). */
constexpr std::uint32_t ms(const std::uint32_t milliseconds) { return milliseconds * 1000U; }
}  // namespace

void setUp() {}
void tearDown() {}

namespace {
void testFirstTapAtZeroIsAcceptedAsAnchor() { TapTempo t; TEST_ASSERT_EQUAL_UINT32(0U, t.registerTap(ms(0U), 20U, 999U)); TEST_ASSERT_EQUAL_UINT32(120U, t.registerTap(ms(500U), 20U, 999U)); }
void testFirstNonzeroTapReturnsNoTempo() { TapTempo t; TEST_ASSERT_EQUAL_UINT32(0U, t.registerTap(ms(1000U), 20U, 999U)); }
void testFiveHundredMillisecondsIs120Bpm() { TapTempo t; t.registerTap(ms(100U),20U,999U); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(ms(600U),20U,999U)); }
void testOneSecondIs60Bpm() { TapTempo t; t.registerTap(ms(100U),20U,999U); TEST_ASSERT_EQUAL_UINT32(60U,t.registerTap(ms(1100U),20U,999U)); }
void testTwoSecondsIs30Bpm() { TapTempo t; t.registerTap(ms(100U),20U,999U); TEST_ASSERT_EQUAL_UINT32(30U,t.registerTap(ms(2100U),20U,999U)); }
void testQuarterSecondIs240Bpm() { TapTempo t; t.registerTap(ms(100U),20U,999U); TEST_ASSERT_EQUAL_UINT32(240U,t.registerTap(ms(350U),20U,999U)); }
void testSixtyMillisecondsClampsTo999Bpm() { TapTempo t; t.registerTap(ms(100U),20U,999U); TEST_ASSERT_EQUAL_UINT32(999U,t.registerTap(ms(160U),20U,999U)); }
void testTapBelowUserMinimumStartsNewSequence() { TapTempo t; t.registerTap(ms(100U),90U,110U); TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(ms(1100U),90U,110U)); }
void testTempoClampsToUserMaximum() { TapTempo t; t.registerTap(ms(100U),90U,110U); TEST_ASSERT_EQUAL_UINT32(110U,t.registerTap(ms(600U),90U,110U)); }
void testZeroMinimumIsRejected() { TapTempo t; TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(ms(100U),0U,120U)); }
void testZeroMaximumIsRejected() { TapTempo t; TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(ms(100U),20U,0U)); }
void testInvertedRangeIsRejected() { TapTempo t; TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(ms(100U),121U,120U)); }
void testDuplicateTapIsRejectedAndResetsHistory() { TapTempo t; t.registerTap(ms(1000U),20U,999U); TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(ms(1000U),20U,999U)); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(ms(1500U),20U,999U)); }
void testTooFastTapResetsHistory() { TapTempo t; t.registerTap(ms(1000U),20U,999U); TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(ms(1000U + clockfw::config::kTapMinimumIntervalMs - 1U),20U,999U)); }
void testTooSlowTapStartsNewSequenceAtMinimumBpmInterval() { TapTempo t; t.registerTap(ms(1000U),20U,999U); TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(ms(4001U),20U,999U)); }
void testExactMinimumBpmIntervalRemainsInSequence() { TapTempo t; t.registerTap(ms(1000U),20U,999U); TEST_ASSERT_EQUAL_UINT32(20U,t.registerTap(ms(4000U),20U,999U)); }
void testSequenceIntervalTracksConfiguredMinimumBpm() { TEST_ASSERT_EQUAL_UINT32(3000U,TapTempo::maximumSequenceIntervalMs(20U)); TEST_ASSERT_EQUAL_UINT32(500U,TapTempo::maximumSequenceIntervalMs(120U)); TEST_ASSERT_EQUAL_UINT32(60000U,TapTempo::maximumSequenceIntervalMs(1U)); TEST_ASSERT_EQUAL_UINT32(60U,TapTempo::maximumSequenceIntervalMs(999U)); TEST_ASSERT_EQUAL_UINT32(0U,TapTempo::maximumSequenceIntervalMs(0U)); }
void testExplicitResetForgetsPreviousTap() { TapTempo t; t.registerTap(ms(1000U),20U,999U); t.reset(); TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(ms(1500U),20U,999U)); }
void testTwoIntervalsAreAveraged() { TapTempo t; t.registerTap(ms(0U),20U,999U); t.registerTap(ms(500U),20U,999U); TEST_ASSERT_EQUAL_UINT32(109U,t.registerTap(ms(1100U),20U,999U)); }
void testThreeIntervalsAreAveraged() { TapTempo t; t.registerTap(ms(0U),20U,999U); t.registerTap(ms(500U),20U,999U); t.registerTap(ms(1000U),20U,999U); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(ms(1500U),20U,999U)); }
void testRollingWindowDropsOldestAfterFourIntervals() { TapTempo t; t.registerTap(ms(0U),20U,999U); t.registerTap(ms(1000U),20U,999U); t.registerTap(ms(1500U),20U,999U); t.registerTap(ms(2000U),20U,999U); t.registerTap(ms(2500U),20U,999U); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(ms(3000U),20U,999U)); }
void testSmallJitterAveragesNear120Bpm() { TapTempo t; t.registerTap(ms(0U),20U,999U); t.registerTap(ms(490U),20U,999U); t.registerTap(ms(1000U),20U,999U); t.registerTap(ms(1495U),20U,999U); const auto bpm=t.registerTap(ms(2005U),20U,999U); TEST_ASSERT_TRUE(bpm>=119U && bpm<=121U); }
void testAlternatingFastSlowTapsAverageStably() { TapTempo t; t.registerTap(ms(0U),20U,999U); t.registerTap(ms(400U),20U,999U); t.registerTap(ms(1000U),20U,999U); t.registerTap(ms(1400U),20U,999U); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(ms(2000U),20U,999U)); }
void testMicrosecondSequenceIntervalMatchesMilliseconds() { TEST_ASSERT_EQUAL_UINT32(3000000U,TapTempo::maximumSequenceIntervalUs(20U)); TEST_ASSERT_EQUAL_UINT32(60000000U,TapTempo::maximumSequenceIntervalUs(1U)); TEST_ASSERT_EQUAL_UINT32(60000U,TapTempo::maximumSequenceIntervalUs(999U)); TEST_ASSERT_EQUAL_UINT32(0U,TapTempo::maximumSequenceIntervalUs(0U)); }
void testTapsAcrossMicrosecondWrapFrom0xFFFFF000() { TapTempo t; std::uint32_t now=0xFFFFF000U; TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(now,20U,999U)); for(int i=0;i<4;++i){ now+=500000U; TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(now,20U,999U)); } TEST_ASSERT_TRUE(now<0xFFFFF000U); }
void testTapsLongAfterSeveralWrapsUseUnsignedDifferences() { TapTempo t; const std::uint32_t start=static_cast<std::uint32_t>(9000000000ULL); TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(start,20U,999U)); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(start+500000U,20U,999U)); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(start+1000000U,20U,999U)); }
void testDuplicateMicrosecondTimestampResetsHistory() { TapTempo t; t.registerTap(0xFFFFFF00U,20U,999U); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(0xFFFFFF00U+500000U,20U,999U)); TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(0xFFFFFF00U+500000U,20U,999U)); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(0xFFFFFF00U+1000000U,20U,999U)); }
void testStaleBoundaryExactMinusOneAndPlusOneMicrosecond() { const std::uint32_t limit=TapTempo::maximumSequenceIntervalUs(20U); const std::uint32_t start=0xFFFFFFFFU-1000000U; { TapTempo t; t.registerTap(start,20U,999U); TEST_ASSERT_EQUAL_UINT32(20U,t.registerTap(start+limit,20U,999U)); } { TapTempo t; t.registerTap(start,20U,999U); TEST_ASSERT_EQUAL_UINT32(20U,t.registerTap(start+limit-1U,20U,999U)); } { TapTempo t; t.registerTap(start,20U,999U); TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(start+limit+1U,20U,999U)); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(start+limit+1U+500000U,20U,999U)); } }
void testTimestampWrapProducesCorrectInterval() { TapTempo t; const std::uint32_t a=std::numeric_limits<std::uint32_t>::max()-249U; TEST_ASSERT_EQUAL_UINT32(0U,t.registerTap(ms(a),20U,999U)); TEST_ASSERT_EQUAL_UINT32(120U,t.registerTap(ms(250U),20U,999U)); }
}

int main(){
    UNITY_BEGIN();
    RUN_TEST(testFirstTapAtZeroIsAcceptedAsAnchor); RUN_TEST(testFirstNonzeroTapReturnsNoTempo);
    RUN_TEST(testFiveHundredMillisecondsIs120Bpm); RUN_TEST(testOneSecondIs60Bpm); RUN_TEST(testTwoSecondsIs30Bpm); RUN_TEST(testQuarterSecondIs240Bpm);
    RUN_TEST(testSixtyMillisecondsClampsTo999Bpm); RUN_TEST(testTapBelowUserMinimumStartsNewSequence); RUN_TEST(testTempoClampsToUserMaximum);
    RUN_TEST(testZeroMinimumIsRejected); RUN_TEST(testZeroMaximumIsRejected); RUN_TEST(testInvertedRangeIsRejected);
    RUN_TEST(testDuplicateTapIsRejectedAndResetsHistory); RUN_TEST(testTooFastTapResetsHistory); RUN_TEST(testTooSlowTapStartsNewSequenceAtMinimumBpmInterval); RUN_TEST(testExactMinimumBpmIntervalRemainsInSequence); RUN_TEST(testSequenceIntervalTracksConfiguredMinimumBpm); RUN_TEST(testExplicitResetForgetsPreviousTap);
    RUN_TEST(testTwoIntervalsAreAveraged); RUN_TEST(testThreeIntervalsAreAveraged); RUN_TEST(testRollingWindowDropsOldestAfterFourIntervals);
    RUN_TEST(testSmallJitterAveragesNear120Bpm); RUN_TEST(testAlternatingFastSlowTapsAverageStably); RUN_TEST(testTimestampWrapProducesCorrectInterval);
    RUN_TEST(testMicrosecondSequenceIntervalMatchesMilliseconds); RUN_TEST(testTapsAcrossMicrosecondWrapFrom0xFFFFF000); RUN_TEST(testTapsLongAfterSeveralWrapsUseUnsignedDifferences); RUN_TEST(testDuplicateMicrosecondTimestampResetsHistory); RUN_TEST(testStaleBoundaryExactMinusOneAndPlusOneMicrosecond);
    return UNITY_END();
}
