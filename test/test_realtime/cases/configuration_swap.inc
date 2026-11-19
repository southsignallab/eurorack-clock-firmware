/**
 * @file configuration_swap.inc
 * @brief Configuration changes compute outside the interrupt lock and only swap under it (F-068).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * 120 BPM in 4/4: one beat is 10000 scheduler ticks; CLOCK x1 serial n fires in
 * tick n * 10000. The swap hook runs scheduler ticks between the preparation and
 * the swap, as the TIM3 interrupt would.
 */

void testConfigurationSwapComputesNoScheduleUnderTheLock() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        configureClockChannel(fixture.state, channel, static_cast<std::uint8_t>(channel + 1U), 10U);
    }
    fixture.begin();
    runTicks(fixture.engine, 3000U);
    const std::uint32_t before = fixture.engine.scheduleCalculationsUnderLockForTest();
    fixture.state.channels[3].common.swingPercent = 30U;
    fixture.engine.updateConfiguration(fixture.state, true);
    fixture.state.channels[5].common.phasePercent = 10U;
    fixture.engine.updateChannel(5U, fixture.state.channels[5], true);
    fixture.state.bpm = 133U;
    fixture.engine.updateConfiguration(fixture.state, false);
    CHECK_EQ(fixture.engine.scheduleCalculationsUnderLockForTest(), before);
    CHECK_EQ(fixture.engine.configurationSwapFallbackCountForTest(), 0U);
}

void testEveryRescheduleCauseWithASchedulerTickInsideTheSwap() {
    // The twelve W-327 causes again, now with one scheduler tick between preparation
    // and swap. Tempo and sync do not prepare a swap and run unchanged.
    gTickInsideConfigurationSwap = true;
    gTicksInsideConfigurationSwap = 0U;
    testCustomGrooveChangeDoesNotRefireLastEvent();
    testCustomGroovePreviewDoesNotRefireLastEvent();
    testSwingIncreaseDoesNotRefireLastEvent();
    testGroovePresetChangeDoesNotRefireLastEvent();
    testGrooveRotationDoesNotRefireLastEvent();
    testPhaseChangeDoesNotRefireLastEvent();
    testRateChangeKeepsTheNextGridEvent();
    testModeAndPatternChangesKeepTheNextGridEvent();
    testTempoChangeDoesNotRefireLastEvent();
    testExternalSyncCorrectionDoesNotRefireOrDropEvents();
    testResetRestartsTheBarExactlyOnce();
    gTickInsideConfigurationSwap = false;
    // Swing, preset, rotation, phase, rate twice and mode swap a configuration;
    // Custom Groove slot, preview, Sequencer pattern and reset swap since F-072.
    CHECK_EQ(gTicksInsideConfigurationSwap, 11U);
}

/** @brief Traces CLOCK x1 with @p change before @p changeTick and @p windowTicks in its swap. */
template <typename Change>
GateTrace traceSwap(
    const std::uint32_t changeTick,
    const std::uint32_t ticks,
    const std::uint32_t windowTicks,
    Fixture& fixture,
    Change change) {
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 1U, 10U);
    fixture.begin();
    GateTrace trace;
    SwapWindowTick window{&fixture.engine, &trace, 0U};
    fixture.engine.setConfigurationSwapHookForTest(runTickInsideSwapWindow, &window);
    traceTicks(fixture.engine, trace, ticks, changeTick, [&] {
        window.remaining = windowTicks;
        change(fixture);
    });
    CHECK(edgesAlternateWithLowTick(trace, 0U));
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    return trace;
}

void testSwapAtTheTickOfAnEventFiresItOnce() {
    const auto phaseChange = [](Fixture& fixture) {
        fixture.state.channels[0].common.phasePercent = 25U;
        fixture.engine.updateChannel(0U, fixture.state.channels[0], true);
    };
    {
        // The window tick is tick 10000: serial 1 fires on the old grid, the
        // preparation is stale and repeats; the new grid continues at 22500.
        Fixture fixture;
        const GateTrace trace = traceSwap(10000U, 25000U, 1U, fixture, phaseChange);
        const auto& rising = trace.rising[0];
        CHECK(std::find(rising.begin(), rising.end(), 10000U) != rising.end());
        CHECK_EQ(countRisesBetween(trace, 0U, 10001U, 22400U), 0U);
        CHECK(hasRiseNear(trace, 0U, 22500U));
        CHECK_EQ(fixture.engine.configurationSwapFallbackCountForTest(), 0U);
    }
    {
        // Without a window tick the change lands before tick 10000: serial 1 moves
        // to 12500 and fires once there, not at 10000.
        Fixture fixture;
        const GateTrace trace = traceSwap(10000U, 25000U, 0U, fixture, phaseChange);
        CHECK_EQ(countRisesBetween(trace, 0U, 9000U, 12400U), 0U);
        CHECK(hasRiseNear(trace, 0U, 12500U));
        CHECK(hasRiseNear(trace, 0U, 22500U));
        CHECK_EQ(trace.rising[0].size(), 3U);
    }
}

void testEventDueInsideTheSwapWindowFiresInTheNextTick() {
    // x1 -> x2 before tick 5000: the new serial 1 lies at 5000. The window tick
    // 5000 still runs the old grid; the swap installs the event, which fires in
    // tick 5001 - once, one tick late, then on the grid again.
    Fixture fixture;
    const GateTrace trace = traceSwap(5000U, 20100U, 1U, fixture, [](Fixture& changed) {
        changed.state.channels[0].common.rate = {ClockRatioMode::Multiply, 2U, 1U, 1U};
        changed.engine.updateChannel(0U, changed.state.channels[0], true);
    });
    const auto& rising = trace.rising[0];
    CHECK(std::find(rising.begin(), rising.end(), 5001U) != rising.end());
    CHECK_EQ(countRisesBetween(trace, 0U, 2U, 20100U), 4U);
    CHECK(hasRiseNear(trace, 0U, 10000U));
    CHECK(hasRiseNear(trace, 0U, 15000U));
    CHECK(hasRiseNear(trace, 0U, 20000U));
}

namespace {
std::uint32_t gGenerationBumps = 0U;
void bumpScheduleGeneration(void* context) {
    // A scheduler-context tempo change in every window invalidates each preparation.
    auto& engine = *static_cast<engine::ClockEngine*>(context);
    ++gGenerationBumps;
    engine.setExternalLockFromIsr(false, 1000U + gGenerationBumps);
}
}  // namespace

void testSwapComputesUnderTheLockWhenTheSchedulerKeepsMovingTheBasis() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 1U, 10U);
    fixture.begin();
    GateTrace trace;
    gGenerationBumps = 0U;
    fixture.engine.setConfigurationSwapHookForTest(bumpScheduleGeneration, &fixture.engine);
    const std::uint32_t before = fixture.engine.scheduleCalculationsUnderLockForTest();
    traceTicks(fixture.engine, trace, 20100U, 2000U, [&] {
        fixture.state.channels[0].common.rate = {ClockRatioMode::Multiply, 2U, 1U, 1U};
        fixture.engine.updateChannel(0U, fixture.state.channels[0], true);
    });
    fixture.engine.setConfigurationSwapHookForTest(nullptr, nullptr);
    CHECK_EQ(gGenerationBumps, 3U);
    CHECK_EQ(fixture.engine.configurationSwapFallbackCountForTest(), 1U);
    CHECK(fixture.engine.scheduleCalculationsUnderLockForTest() > before);
    // The change still applies: x2 from tick 5000.
    CHECK(hasRiseNear(trace, 0U, 5000U));
    CHECK(hasRiseNear(trace, 0U, 10000U));
    CHECK(hasRiseNear(trace, 0U, 15000U));
    CHECK_EQ(trace.rising[0].size(), 5U);
}
