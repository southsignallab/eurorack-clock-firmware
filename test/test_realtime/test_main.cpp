/**
 * @file test_main.cpp
 * @brief Focused integration and stress tests for CLOCK real-time timing, SYNC, and RST behavior.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <map>
#include <stdexcept>
#include <unity.h>
#include <Arduino.h>

#include "clock_core.h"
#include "config.h"
#include "defaults.h"
#include "domain/clock_types.h"
#include "domain/sequencer_pattern.h"
#include "domain/sequencer_step_metadata.h"
#include "domain/default_configuration.h"
#include "engine/clock_engine.h"
#include "hal/exti_dispatch.h"
#include "hal/external_input_capture.h"
#include "hal/gate_output_driver.h"
#include "hal/interrupt_lock.h"
#include "hal/platform_io.h"
#include "pin_map.h"
#include "services/external_sync_controller.h"

using namespace clockfw;

void setUp() {}
void tearDown() {}

namespace {

std::uint32_t checks = 0U;

#define CHECK(condition) do { ++checks; TEST_ASSERT_TRUE(condition); } while (false)
#define CHECK_EQ(actual, expected) do { ++checks; TEST_ASSERT_TRUE((actual) == (expected)); } while (false)

ClockState makeRealtimeState() {
    ClockState state{};
    initializeFactoryDefaults(state);
    state.operatingMode = OperatingMode::Independent;
    state.source = ClockSource::Auto;
    for (ChannelConfig& channel : state.channels) {
        channel.common.mode = ChannelMode::Off;
    }
    state.channels[0].common.mode = ChannelMode::Clock;
    state.channels[0].common.probabilityPercent = 100U;
    state.channels[0].common.resetMode = ResetMode::Global;
    return state;
}

struct Fixture final {
    hal::GateOutputDriver gates;
    engine::ClockEngine engine;
    hal::ExternalInputCapture inputs;
    services::ExternalSyncController sync;
    ClockState state;

    Fixture()
        : engine(gates), sync(inputs, engine), state(makeRealtimeState()) {
        fakefw::resetArduino();
        gates.beginDisabled();
        gates.enableOutputStage();
    }

    void begin() {
        engine.begin(state);
        sync.begin(state);
        engine.play();
    }

    void tick(std::uint32_t nowUs) {
        engine.beginSchedulerQuantumFromIsr();
        sync.processSchedulerTick(nowUs);
        engine.processSchedulerTick();
    }
};

void runTicks(engine::ClockEngine& engine, const std::uint32_t count) {
    for (std::uint32_t index = 0U; index < count; ++index) {
        engine.processSchedulerTick();
    }
}

#include "cases/clock_precount_sync.inc"
#include "cases/reset_overflow_boundaries.inc"
#include "cases/sequencer_step_expression.inc"
#include "cases/gate_output_batching.inc"
#include "cases/reschedule_causes.inc"
#include "cases/sync_phase_alignment.inc"
#include "cases/scheduler_catch_up.inc"
#include "cases/configuration_swap.inc"
#include "cases/staged_swaps.inc"
#include "cases/exti_dispatch.inc"
#include "cases/isr_transport.inc"

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(testMinuteLongMasterTimingHasNoDrift);
    RUN_TEST(testConfiguredGateLengthsReachPhysicalGpio);
    RUN_TEST(testInternalPreCountSuppressesGatesAndStartsFromPhaseZero);
    RUN_TEST(testPreCountPauseResumeContinuesInsteadOfRestarting);
    RUN_TEST(testPreCountStopRestartsFullCountOnNextPlay);
    RUN_TEST(testExternalPreCountFollowsMasterMeterBeatUnitAcrossPpqn);
    RUN_TEST(testRepresentativeExternalTemposAndPpqn);
    RUN_TEST(testFallingEdgeSelection);
    RUN_TEST(testGlitchesDoNotCorruptPeriodEstimator);
    RUN_TEST(testDeterministicJitterRemainsBounded);
    RUN_TEST(testSyncTimeoutBoundaryAndLossModes);
    RUN_TEST(testSlowExternalClockDoesNotTimeoutBeforeSecondPulse);
    RUN_TEST(testTimestampWraparound);
    RUN_TEST(testExternalPulsesWhileStoppedDoNotShiftNextStart);
    RUN_TEST(testPauseIgnoresPulsesAndResumesWithoutBurst);
    RUN_TEST(testResetTriggerIsEdgeTriggered);
    RUN_TEST(testResetBurstIsCoalescedPerSchedulerQuantum);
    RUN_TEST(testResetOverflowStillCoalescesToOneReset);
    RUN_TEST(testResetGateHoldsAndReleasesScheduler);
    RUN_TEST(testGateModeHonorsAlreadyHighInputAndRuntimeModeChange);
    RUN_TEST(testResetWinsWhenSyncArrivesOnSameSchedulerBoundary);
    RUN_TEST(testPhysicalComparatorIrqPathWhenPinsAreAssigned);
    RUN_TEST(testInputQueuesPublishStableContinuityBoundaryOnOverflow);
    RUN_TEST(testOverflowMarkerCannotCreateCrossGapTempo);
    RUN_TEST(testTechnicalMaximumRejectsImpossibleSyncRate);
    RUN_TEST(testUnchangedSyncConfigurationDoesNotMaskInterrupts);
    RUN_TEST(testExplicitExternalLockClearAndReacquire);
    RUN_TEST(testSyncQueueOverflowDoesNotMasqueradeAsTempoDrop);
    RUN_TEST(testExternalSyncConfigurationBoundaryPaths);
    RUN_TEST(testEngineExternalSyncDefensiveConfigurationPaths);
    RUN_TEST(testSequencerStepProbabilityOverrideReplacesChannelProbability);
    RUN_TEST(testSequencerEvenRatchetEmitsBoundedDistinctSubEvents);
    RUN_TEST(testSequencerEightWayRatchetEmitsEightBoundedSubEvents);
    RUN_TEST(testSequencerTieKeepsAdjacentGateContinuousThenReleases);
    RUN_TEST(testSequencerTieDoesNotCrossRestRandomOrOnceEnd);
    RUN_TEST(testSequencerLiveEditReleasesActiveTieImmediately);
    RUN_TEST(testSequencerRelativeDutyOverridesGateLength);
    RUN_TEST(testCoincidentGateEdgesUseOneRegisterWritePerTick);
    RUN_TEST(testRisingAndFallingEdgesOfDifferentChannelsShareOneWrite);
    RUN_TEST(testOffChannelStaysLowAndModeOffEndsGateImmediately);
    RUN_TEST(testTieKeepsGateHighAcrossStepsWithOneWritePerTick);
    RUN_TEST(testRatchetSubEventsKeepDistinctEdgesWithOneWritePerTick);
    RUN_TEST(testMutedOutputStageKeepsGatesLowWhileEngineRuns);
    RUN_TEST(testGatesStayLowFromBootUntilPlay);
    RUN_TEST(testGateDriverWritesSeveralChannelsInOneAccess);
    RUN_TEST(testTempoJumpDuringHighGateKeepsEveryRisingEdge);
    RUN_TEST(testLivePatternChangeDuringHighGateKeepsTheNextEdge);
    RUN_TEST(testCustomGrooveChangeDoesNotRefireLastEvent);
    RUN_TEST(testCustomGroovePreviewDoesNotRefireLastEvent);
    RUN_TEST(testSwingIncreaseDoesNotRefireLastEvent);
    RUN_TEST(testGroovePresetChangeDoesNotRefireLastEvent);
    RUN_TEST(testGrooveRotationDoesNotRefireLastEvent);
    RUN_TEST(testPhaseChangeDoesNotRefireLastEvent);
    RUN_TEST(testRateChangeKeepsTheNextGridEvent);
    RUN_TEST(testModeAndPatternChangesKeepTheNextGridEvent);
    RUN_TEST(testTempoChangeDoesNotRefireLastEvent);
    RUN_TEST(testExternalSyncCorrectionDoesNotRefireOrDropEvents);
    RUN_TEST(testResetRestartsTheBarExactlyOnce);
    RUN_TEST(testRescheduleInSameTickAsHighGateMovesRiseByOneTick);
    RUN_TEST(testOnTimePulseAfterEarlyPulsesFiresTheBeatOnThePulse);
    RUN_TEST(testEarlyPulseBeyondTheEpochKeepsTheFollowingBeats);
    RUN_TEST(testLatePulseMovesTheGridWithoutRepeatingTheBeat);
    RUN_TEST(testOnTimePulsesKeepTheGridUnchanged);
    RUN_TEST(testOutlierPulseRealignsOnceAndTheGridFollowsTheNextPulses);
    RUN_TEST(testFreeChannelIgnoresEarlyAndLatePulseCorrections);
    RUN_TEST(testLockOverOneTwoAndSevenUpdatesKeepsTheTimelineExact);
    RUN_TEST(testCatchUpLimitExactlyBelowAndAbove);
    RUN_TEST(testLockAcrossTheTim5WrapKeepsTheTimelineExact);
    RUN_TEST(testAlreadyCountedUpdateAdvancesNothing);
    RUN_TEST(testTempoStaysExactOverAMinuteOfRandomLocksAndJitter);
    RUN_TEST(testEventsInCaughtUpTicksFireOnceAndInOrder);
    RUN_TEST(testGateLengthCountsCaughtUpTicks);
    RUN_TEST(testConfigurationSwapComputesNoScheduleUnderTheLock);
    RUN_TEST(testEveryRescheduleCauseWithASchedulerTickInsideTheSwap);
    RUN_TEST(testSwapAtTheTickOfAnEventFiresItOnce);
    RUN_TEST(testEventDueInsideTheSwapWindowFiresInTheNextTick);
    RUN_TEST(testSwapComputesUnderTheLockWhenTheSchedulerKeepsMovingTheBasis);
    RUN_TEST(testStagedPathsComputeNoScheduleUnderTheLock);
    RUN_TEST(testEveryStagedPathSwapsAfterItsPreparation);
    RUN_TEST(testGrooveSlotSwapAtTheTickOfAnEventFiresItOnce);
    RUN_TEST(testSequencerPatternSwapAtTheTickOfAnEventFiresItOnce);
    RUN_TEST(testResetSwapAtTheTickOfAnEventRestartsTheBarOnce);
    RUN_TEST(testStopAndPlayWithATickInsideTheSwapLoseAndDoubleNothing);
    RUN_TEST(testStopLowersEveryHighGateInOneWrite);
    RUN_TEST(testStagedPathsComputeUnderTheLockWhenTheSchedulerKeepsMovingTheBasis);
    RUN_TEST(testCaptureEstimatesMatchTheSchedulerTimeline);
    std::cout << "Realtime assertions: " << checks << "\n";
    RUN_TEST(testExti15To10DispatchEndsAndServesLine15);
    RUN_TEST(testExti9To5DispatchServesTheInputLines);
    RUN_TEST(testSettledLevelReadsAgainAfterAnEdgeInTheWindow);
    RUN_TEST(testInputTransportRolesOpenNoLockInTheSchedulerTick);
    RUN_TEST(testSyncLockAndLossOpenNoLockInTheSchedulerTick);
    RUN_TEST(testClosingTheCommitWindowStartsOutsideItsLock);
    RUN_TEST(testInputStopDuringTheDeferredStartPreparationWins);
    return UNITY_END();
}
