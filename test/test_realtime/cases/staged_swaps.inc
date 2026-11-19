/**
 * @file staged_swaps.inc
 * @brief Groove, preview, Sequencer pattern and transport changes prepare outside the lock (F-072).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * 120 BPM in 4/4: one beat is 10000 scheduler ticks; CLOCK x1 serial n fires in
 * tick n * 10000 (serial 0 in tick 1), SEQ x1 runs on sixteenths (2500 ticks).
 * The swap hook runs scheduler ticks between the preparation and the swap, as
 * the TIM3 interrupt would.
 */

/** Channel 7 runs the Sequencer in prepareStagedPathFixture(). */
constexpr std::size_t kSequencerChannel = kChannelCount - 1U;

/** @brief Schedule calculations under the lock while @p change runs. */
template <typename Change>
std::uint32_t calculationsUnderLockDuring(Fixture& fixture, Change change) {
    const std::uint32_t before = fixture.engine.scheduleCalculationsUnderLockForTest();
    change();
    return fixture.engine.scheduleCalculationsUnderLockForTest() - before;
}

/** @brief Seven CLOCK channels on Custom Groove slot 0 and one Sequencer channel. */
void prepareStagedPathFixture(Fixture& fixture) {
    prepareInternalFixture(fixture);
    for (std::size_t channel = 0U; channel + 1U < kChannelCount; ++channel) {
        configureClockChannel(fixture.state, channel, static_cast<std::uint8_t>(channel + 1U), 10U);
        fixture.state.channels[channel].common.groove = {GroovePreset::Custom, 100U, 0U, 0U};
    }
    ChannelConfig& sequencer = fixture.state.channels[kSequencerChannel];
    sequencer.common.mode = ChannelMode::Sequencer;
    sequencer.common.probabilityPercent = 100U;
    sequencer.common.resetMode = ResetMode::Global;
    sequencer.common.gateLengthMs = 10U;
}

SequencerPatternV2 patternWithGates(const std::uint64_t gates, const std::uint8_t rotation) {
    SequencerPatternV2 pattern{};
    pattern.length = 4U;
    pattern.gates = {{gates, 0ULL}};
    pattern.rotation = rotation;
    return pattern;
}

CustomGroovePattern opposedGroove() {
    CustomGroovePattern opposed{};
    opposed.length = 2U;
    opposed.offsets256[0] = -120;
    opposed.offsets256[1] = 120;
    return opposed;
}

void testStagedPathsComputeNoScheduleUnderTheLock() {
    Fixture fixture;
    prepareStagedPathFixture(fixture);
    fixture.begin();
    fixture.engine.updateCustomGrooveSlot(0U, CustomGroovePattern{}, false);
    fixture.engine.updateSequencerPattern(
        kSequencerChannel, patternWithGates(0xFULL, 0U), false);
    runTicks(fixture.engine, 3000U);
    CHECK_EQ(calculationsUnderLockDuring(fixture, [&] {
        fixture.engine.updateCustomGrooveSlot(0U, opposedGroove(), true);
    }), 0U);
    CHECK_EQ(calculationsUnderLockDuring(fixture, [&] {
        fixture.engine.setCustomGroovePreview(2U, opposedGroove(), 100U, 1U, true);
    }), 0U);
    CHECK_EQ(calculationsUnderLockDuring(fixture, [&] {
        fixture.engine.clearCustomGroovePreview(2U, true);
    }), 0U);
    CHECK_EQ(calculationsUnderLockDuring(fixture, [&] {
        fixture.engine.updateSequencerPattern(
            kSequencerChannel, patternWithGates(0x5ULL, 1U), true);
    }), 0U);
    CHECK_EQ(calculationsUnderLockDuring(fixture, [&] { fixture.engine.resetGlobalPhase(); }), 0U);
    CHECK_EQ(calculationsUnderLockDuring(fixture, [&] { fixture.engine.stop(); }), 0U);
    CHECK_EQ(calculationsUnderLockDuring(fixture, [&] { fixture.engine.play(); }), 0U);
    runTicks(fixture.engine, 100U);
    CHECK_EQ(calculationsUnderLockDuring(fixture, [&] {
        (void)fixture.engine.nominalIntervalQ32(0U);
    }), 0U);
    CHECK_EQ(calculationsUnderLockDuring(fixture, [&] {
        (void)fixture.engine.estimateMasterPositionAtUs(1'000U, 26'000U);
    }), 0U);
    CHECK_EQ(fixture.engine.configurationSwapFallbackCountForTest(), 0U);
}

namespace {
std::uint32_t gStagedSwapWindows = 0U;
void countStagedSwapWindow(void*) {
    ++gStagedSwapWindows;
}
}  // namespace

void testEveryStagedPathSwapsAfterItsPreparation() {
    Fixture fixture;
    prepareStagedPathFixture(fixture);
    fixture.begin();
    runTicks(fixture.engine, 3000U);
    fixture.engine.setConfigurationSwapHookForTest(countStagedSwapWindow, nullptr);
    const auto windowsDuring = [&](auto change) {
        gStagedSwapWindows = 0U;
        change();
        return gStagedSwapWindows;
    };
    engine::ClockEngine& engine = fixture.engine;
    CHECK_EQ(windowsDuring([&] { engine.updateCustomGrooveSlot(0U, opposedGroove(), true); }), 1U);
    CHECK_EQ(windowsDuring([&] {
        engine.setCustomGroovePreview(1U, opposedGroove(), 50U, 0U, true);
    }), 1U);
    CHECK_EQ(windowsDuring([&] { fixture.engine.clearCustomGroovePreview(1U, true); }), 1U);
    CHECK_EQ(windowsDuring([&] {
        fixture.engine.updateSequencerPattern(
            kSequencerChannel, patternWithGates(0x3ULL, 0U), true);
    }), 1U);
    CHECK_EQ(windowsDuring([&] { fixture.engine.resetGlobalPhase(); }), 1U);
    CHECK_EQ(windowsDuring([&] { fixture.engine.stop(); }), 1U);
    CHECK_EQ(windowsDuring([&] { fixture.engine.play(); }), 1U);
    // Resuming from PAUSE restarts nothing and therefore prepares nothing.
    fixture.engine.pause();
    CHECK_EQ(windowsDuring([&] { fixture.engine.play(); }), 0U);
    fixture.engine.setConfigurationSwapHookForTest(nullptr, nullptr);
}

/** @brief Traces channel 0 with @p configure before begin(), @p change before @p changeTick. */
template <typename Configure, typename Change>
GateTrace traceStagedSwap(
    const std::uint32_t changeTick,
    const std::uint32_t ticks,
    const std::uint32_t windowTicks,
    Fixture& fixture,
    Configure configure,
    Change change) {
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 1U, 10U);
    configure(fixture);
    fixture.begin();
    GateTrace trace;
    SwapWindowTick window{&fixture.engine, &trace, 0U};
    fixture.engine.setConfigurationSwapHookForTest(runTickInsideSwapWindow, &window);
    traceTicks(fixture.engine, trace, ticks, changeTick, [&] {
        window.remaining = windowTicks;
        change(fixture);
    });
    fixture.engine.setConfigurationSwapHookForTest(nullptr, nullptr);
    CHECK(edgesAlternateWithLowTick(trace, 0U));
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    return trace;
}

void testGrooveSlotSwapAtTheTickOfAnEventFiresItOnce() {
    const auto straightSlot = [](Fixture& fixture) {
        fixture.state.channels[0].common.groove = {GroovePreset::Custom, 100U, 0U, 0U};
        CustomGroovePattern straight{};
        straight.length = 2U;
        fixture.engine.updateCustomGrooveSlot(0U, straight, false);
    };
    const auto opposedSlot = [](Fixture& fixture) {
        fixture.engine.updateCustomGrooveSlot(0U, opposedGroove(), true);
    };
    {
        // The window tick is tick 10000: serial 1 fires on the straight grid, the
        // preparation is stale and repeats; serial 2 follows at 15312.
        Fixture fixture;
        const GateTrace trace =
            traceStagedSwap(10000U, 36000U, 1U, fixture, straightSlot, opposedSlot);
        const auto& rising = trace.rising[0];
        CHECK(std::find(rising.begin(), rising.end(), 10000U) != rising.end());
        CHECK_EQ(countRisesBetween(trace, 0U, 10001U, 15300U), 0U);
        CHECK(hasRiseNear(trace, 0U, 15312U));
        CHECK(hasRiseNear(trace, 0U, 34687U));
        CHECK(hasRiseNear(trace, 0U, 35312U));
        CHECK_EQ(fixture.engine.configurationSwapFallbackCountForTest(), 0U);
    }
    {
        // Without a window tick serial 1 moves to 14687 and fires once there.
        Fixture fixture;
        const GateTrace trace =
            traceStagedSwap(10000U, 36000U, 0U, fixture, straightSlot, opposedSlot);
        CHECK_EQ(countRisesBetween(trace, 0U, 9000U, 14600U), 0U);
        CHECK(hasRiseNear(trace, 0U, 14687U));
        CHECK(hasRiseNear(trace, 0U, 15312U));
        CHECK_EQ(trace.rising[0].size(), 5U);
    }
}

void testSequencerPatternSwapAtTheTickOfAnEventFiresItOnce() {
    const auto allSteps = [](Fixture& fixture) {
        fixture.state.channels[0].common.mode = ChannelMode::Sequencer;
        fixture.engine.updateSequencerPattern(0U, patternWithGates(0xFULL, 0U), false);
    };
    const auto evenSteps = [](Fixture& fixture) {
        fixture.engine.updateSequencerPattern(0U, patternWithGates(0x5ULL, 0U), true);
    };
    {
        // Window tick 7500 plays step 3 of the old pattern once; the new pattern
        // then rests on steps 1 and 3 (12500, 17500).
        Fixture fixture;
        const GateTrace trace = traceStagedSwap(7500U, 20100U, 1U, fixture, allSteps, evenSteps);
        const auto& rising = trace.rising[0];
        CHECK_EQ(std::count(rising.begin(), rising.end(), 7500U), 1);
        CHECK(hasRiseNear(trace, 0U, 10000U));
        CHECK_EQ(countRisesBetween(trace, 0U, 10010U, 14990U), 0U);
        CHECK(hasRiseNear(trace, 0U, 15000U));
        CHECK_EQ(countRisesBetween(trace, 0U, 15010U, 19990U), 0U);
        CHECK(hasRiseNear(trace, 0U, 20000U));
    }
    {
        // Without a window tick step 3 already rests under the new pattern.
        Fixture fixture;
        const GateTrace trace = traceStagedSwap(7500U, 20100U, 0U, fixture, allSteps, evenSteps);
        CHECK_EQ(countRisesBetween(trace, 0U, 5010U, 9990U), 0U);
        CHECK(hasRiseNear(trace, 0U, 10000U));
        CHECK(hasRiseNear(trace, 0U, 15000U));
        CHECK(hasRiseNear(trace, 0U, 20000U));
    }
}

void testResetSwapAtTheTickOfAnEventRestartsTheBarOnce() {
    const auto none = [](Fixture&) {};
    const auto reset = [](Fixture& fixture) { fixture.engine.resetGlobalPhase(); };
    {
        // Window tick 10000 plays beat 2 of the old bar; the reset then lowers
        // that gate at once and replays beat 1 one tick later (rise moved by one
        // tick, F-011). The new bar continues one beat after the reset.
        Fixture fixture;
        const GateTrace trace = traceStagedSwap(10000U, 20100U, 1U, fixture, none, reset);
        const auto& rising = trace.rising[0];
        CHECK(std::find(rising.begin(), rising.end(), 10000U) != rising.end());
        CHECK(std::find(rising.begin(), rising.end(), 10002U) != rising.end());
        CHECK_EQ(countRisesBetween(trace, 0U, 9000U, 19900U), 2U);
        CHECK(hasRiseNear(trace, 0U, 20000U));
        CHECK_EQ(fixture.engine.resetRuntimeCountForTest(), 2U);
    }
    {
        // Without a window tick the reset lands before tick 10000: beat 1 replays
        // in tick 10000 instead of beat 2, once.
        Fixture fixture;
        const GateTrace trace = traceStagedSwap(10000U, 20100U, 0U, fixture, none, reset);
        CHECK_EQ(countRisesBetween(trace, 0U, 9000U, 19900U), 1U);
        CHECK(hasRiseNear(trace, 0U, 10000U));
        CHECK(hasRiseNear(trace, 0U, 20000U));
    }
}

void testStopAndPlayWithATickInsideTheSwapLoseAndDoubleNothing() {
    Fixture fixture;
    GateTrace trace = traceStagedSwap(5000U, 7000U, 1U, fixture, [](Fixture&) {},
                                      [](Fixture& stopped) { stopped.engine.stop(); });
    CHECK(!fixture.engine.isPlaying());
    SwapWindowTick window{&fixture.engine, &trace, 0U};
    fixture.engine.setConfigurationSwapHookForTest(runTickInsideSwapWindow, &window);
    traceTicks(fixture.engine, trace, 12000U, 7100U, [&] {
        window.remaining = 1U;
        fixture.engine.play();
    });
    fixture.engine.setConfigurationSwapHookForTest(nullptr, nullptr);
    CHECK(edgesAlternateWithLowTick(trace, 0U));
    // Serial 0 in tick 1, none while stopped, then beat 1 once right after the
    // window tick and beat 2 one beat later.
    CHECK_EQ(countRisesBetween(trace, 0U, 2U, 7099U), 0U);
    CHECK_EQ(countRisesBetween(trace, 0U, 7100U, 7110U), 1U);
    CHECK(hasRiseNear(trace, 0U, 7101U));
    CHECK(hasRiseNear(trace, 0U, 17101U));
    CHECK_EQ(trace.rising[0].size(), 3U);
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
}

void testStopLowersEveryHighGateInOneWrite() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        configureClockChannel(fixture.state, channel, 1U, 10U);
    }
    fixture.begin();
    runTicks(fixture.engine, 50U);  // serial 0 of every channel is HIGH for 200 ticks
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK(fixture.gates.channelStateHigh(channel));
    }
    hal::platform::resetPortWriteLogForTest();
    fixture.engine.stop();
    CHECK_EQ(hal::platform::portWriteCountForTest(), 1U);
    CHECK_EQ(hal::platform::lastPortWriteForTest().resetMask, allGateLines());
    CHECK_EQ(hal::platform::lastPortWriteForTest().setMask, 0U);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK(!fixture.gates.channelStateHigh(channel));
    }
    // Stopped: nothing rises again.
    GateTrace trace;
    traceTicks(fixture.engine, trace, 20000U);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK(trace.rising[channel].empty());
    }
}

void testStagedPathsComputeUnderTheLockWhenTheSchedulerKeepsMovingTheBasis() {
    const auto bumpingChange = [](auto change) {
        Fixture fixture;
        prepareStagedPathFixture(fixture);
        fixture.begin();
        fixture.engine.updateSequencerPattern(
            kSequencerChannel, patternWithGates(0xFULL, 0U), false);
        runTicks(fixture.engine, 3000U);
        gGenerationBumps = 0U;
        fixture.engine.setConfigurationSwapHookForTest(bumpScheduleGeneration, &fixture.engine);
        const std::uint32_t before = fixture.engine.scheduleCalculationsUnderLockForTest();
        change(fixture);
        fixture.engine.setConfigurationSwapHookForTest(nullptr, nullptr);
        CHECK_EQ(gGenerationBumps, 3U);
        CHECK_EQ(fixture.engine.configurationSwapFallbackCountForTest(), 1U);
        CHECK(fixture.engine.scheduleCalculationsUnderLockForTest() > before);
        return fixture.engine.snapshot();
    };
    (void)bumpingChange([](Fixture& fixture) {
        fixture.engine.updateCustomGrooveSlot(0U, opposedGroove(), true);
        CHECK_EQ(fixture.engine.customGrooveSlotForTest(0U).offsets256[1], 120);
    });
    (void)bumpingChange([](Fixture& fixture) {
        fixture.engine.setCustomGroovePreview(3U, opposedGroove(), 100U, 0U, true);
    });
    (void)bumpingChange([](Fixture& fixture) {
        fixture.engine.updateSequencerPattern(
            kSequencerChannel, patternWithGates(0x5ULL, 2U), true);
        CHECK_EQ(fixture.engine.sequencerPatternForTest(kSequencerChannel).rotation, 2U);
    });
    const engine::EngineSnapshot afterReset = bumpingChange([](Fixture& fixture) {
        fixture.engine.resetGlobalPhase();
    });
    // The reset still applied: beat 1 of bar 1 at the reset position.
    CHECK_EQ(afterReset.masterBeat, 1U);
    CHECK_EQ(afterReset.masterBar, 1U);
    CHECK_EQ(afterReset.masterBeatPhaseQ32, 0U);
}

void testCaptureEstimatesMatchTheSchedulerTimeline() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 1U, 10U);
    fixture.begin();
    runTicks(fixture.engine, 3000U);
    // CLOCK x1 in 4/4: one beat per event.
    CHECK_EQ(fixture.engine.nominalIntervalQ32(0U), core::kQ32One);
    std::uint64_t remainder = 0U;
    const std::uint64_t incrementQ32 = core::calculateMasterIncrementMilliBpmQ32(
        120000U, 4U, config::kSchedulerFrequencyHz, remainder);
    const std::uint64_t positionQ32 = fixture.engine.snapshot().masterPositionQ32;
    // 25 ms before now are 500 ticks; the bound is 100 ms.
    CHECK_EQ(fixture.engine.estimateMasterPositionAtUs(1'000U, 26'000U),
             positionQ32 - 500U * incrementQ32);
    CHECK_EQ(fixture.engine.estimateMasterPositionAtUs(0U, 300'000U),
             positionQ32 - 2000U * incrementQ32);
}
