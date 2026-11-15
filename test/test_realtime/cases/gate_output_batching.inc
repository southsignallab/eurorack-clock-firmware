/**
 * @file gate_output_batching.inc
 * @brief One GPIOB register write per scheduler tick and no lost gate edges (F-011, F-015).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

/** @brief GPIOB line mask of a set of gate channels. */
std::uint16_t gateLines(const std::initializer_list<std::size_t> channels) {
    std::uint16_t mask = 0U;
    for (const std::size_t channel : channels) {
        mask = static_cast<std::uint16_t>(mask | pinmap::pinLineMask(pinmap::kGateChannelPins[channel]));
    }
    return mask;
}

std::uint16_t allGateLines() {
    return gateLines({0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U});
}

std::size_t gateChannelForPin(const std::uint32_t pin) {
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        if (pinmap::kGateChannelPins[channel] == pin) {
            return channel;
        }
    }
    return kChannelCount;
}

/**
 * @brief Gate activity recorded from the GPIO fake.
 *
 * Scheduler edges carry the number of the tick that wrote them (counted from 1).
 * Foreground edges written before tick N carry N: a rise in tick N after such a
 * fall therefore shows as "no LOW tick", which is what a real output would see.
 */
struct GateTrace {
    std::array<std::vector<std::uint32_t>, kChannelCount> rising{};
    std::array<std::vector<std::uint32_t>, kChannelCount> falling{};
    std::map<std::uint32_t, hal::platform::PortWriteRecord> portWrites{};
    std::uint32_t ticks = 0U;
    std::uint32_t ticksWithoutSinglePortWrite = 0U;
    std::uint32_t overlappingMasks = 0U;
    std::uint16_t linesEverSet = 0U;
};

void recordGateWrites(GateTrace& trace, const std::uint32_t tick) {
    for (const auto& write : fakefw::writes) {
        const std::size_t channel = gateChannelForPin(write.pin);
        if (channel < kChannelCount) {
            (write.value == HIGH ? trace.rising : trace.falling)[channel].push_back(tick);
        }
    }
    fakefw::writes.clear();
}

/** @brief Runs and records one scheduler tick as tick trace.ticks + 1. */
void traceOneTick(engine::ClockEngine& engine, GateTrace& trace) {
    const std::uint32_t tick = trace.ticks + 1U;
    hal::platform::resetPortWriteLogForTest();
    engine.processSchedulerTick();
    trace.ticks = tick;
    if (hal::platform::portWriteCountForTest() != 1U) {
        ++trace.ticksWithoutSinglePortWrite;
    }
    const hal::platform::PortWriteRecord record = hal::platform::lastPortWriteForTest();
    if ((record.setMask & record.resetMask) != 0U) {
        ++trace.overlappingMasks;
    }
    if (record.setMask != 0U || record.resetMask != 0U) {
        trace.portWrites[tick] = record;
    }
    trace.linesEverSet = static_cast<std::uint16_t>(trace.linesEverSet | record.setMask);
    recordGateWrites(trace, tick);
}

/** @brief Runs @p count scheduler ticks; @p action runs once in the foreground before tick @p actionTick. */
template <typename Action>
void traceTicks(
    engine::ClockEngine& engine,
    GateTrace& trace,
    const std::uint32_t count,
    const std::uint32_t actionTick,
    Action action) {
    for (std::uint32_t index = 0U; index < count; ++index) {
        fakefw::writes.clear();
        if (trace.ticks + 1U == actionTick) {
            action();
            recordGateWrites(trace, trace.ticks + 1U);
        }
        traceOneTick(engine, trace);
    }
}

void traceTicks(engine::ClockEngine& engine, GateTrace& trace, const std::uint32_t count) {
    traceTicks(engine, trace, count, 0U, [] {});
}

/** @brief Every rise follows a fall at least one tick earlier, and edges alternate. */
bool edgesAlternateWithLowTick(const GateTrace& trace, const std::size_t channel) {
    const auto& rising = trace.rising[channel];
    const auto& falling = trace.falling[channel];
    std::size_t fall = 0U;
    for (std::size_t rise = 0U; rise < rising.size(); ++rise) {
        if (rise == 0U) {
            continue;
        }
        // Exactly one fall between two rises, strictly before the second rise.
        while (fall < falling.size() && falling[fall] <= rising[rise - 1U]) {
            ++fall;
        }
        if (fall >= falling.size() || falling[fall] >= rising[rise]) {
            return false;
        }
        if (fall + 1U < falling.size() && falling[fall + 1U] < rising[rise]) {
            return false;
        }
    }
    return true;
}

std::size_t countRisesBetween(
    const GateTrace& trace,
    const std::size_t channel,
    const std::uint32_t firstTick,
    const std::uint32_t lastTick) {
    return static_cast<std::size_t>(std::count_if(
        trace.rising[channel].begin(), trace.rising[channel].end(),
        [&](const std::uint32_t tick) { return tick >= firstTick && tick <= lastTick; }));
}

bool hasRiseNear(const GateTrace& trace, const std::size_t channel, const std::uint32_t tick) {
    return countRisesBetween(trace, channel, tick > 2U ? tick - 2U : 0U, tick + 2U) == 1U;
}

void configureClockChannel(
    ClockState& state,
    const std::size_t channel,
    const std::uint8_t factor,
    const std::uint16_t gateLengthMs) {
    ChannelConfig& config = state.channels[channel];
    config.common.mode = ChannelMode::Clock;
    config.common.probabilityPercent = 100U;
    config.common.resetMode = ResetMode::Global;
    config.common.rate = {ClockRatioMode::Multiply, factor, 1U, 1U};
    config.common.gateLengthMs = gateLengthMs;
    config.clock.meter = {4U, 4U};
}

void prepareInternalFixture(Fixture& fixture) {
    fixture.state.source = ClockSource::Internal;
    fixture.state.bpm = 120U;
    hal::platform::resetPortWriteLogForTest();
}

void testCoincidentGateEdgesUseOneRegisterWritePerTick() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        configureClockChannel(fixture.state, channel, 1U, 10U);
    }
    fixture.begin();
    GateTrace trace;
    traceTicks(fixture.engine, trace, 20000U);

    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    CHECK_EQ(trace.overlappingMasks, 0U);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK_EQ(trace.rising[channel], trace.rising[0]);
        CHECK_EQ(trace.falling[channel], trace.falling[0]);
    }
    CHECK_EQ(trace.rising[0].size(), 3U);
    for (const std::uint32_t tick : trace.rising[0]) {
        CHECK_EQ(trace.portWrites[tick].setMask, allGateLines());
        CHECK_EQ(trace.portWrites[tick].resetMask, 0U);
    }
    for (const std::uint32_t tick : trace.falling[0]) {
        CHECK_EQ(trace.portWrites[tick].resetMask, allGateLines());
        CHECK_EQ(trace.portWrites[tick].setMask, 0U);
    }
}

void testRisingAndFallingEdgesOfDifferentChannelsShareOneWrite() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    // CLOCK x1 with a 250 ms gate falls exactly when the CLOCK x2 offbeat rises.
    configureClockChannel(fixture.state, 0U, 1U, 250U);
    configureClockChannel(fixture.state, 1U, 2U, 1U);
    fixture.begin();
    GateTrace trace;
    traceTicks(fixture.engine, trace, 16000U);

    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    CHECK_EQ(trace.overlappingMasks, 0U);
    CHECK(trace.falling[0].size() >= 2U);
    const std::uint32_t sharedTick = trace.falling[0][1];
    CHECK(hasRiseNear(trace, 1U, sharedTick));
    CHECK(std::find(trace.rising[1].begin(), trace.rising[1].end(), sharedTick) != trace.rising[1].end());
    CHECK_EQ(trace.portWrites[sharedTick].setMask, gateLines({1U}));
    CHECK_EQ(trace.portWrites[sharedTick].resetMask, gateLines({0U}));
}

void testOffChannelStaysLowAndModeOffEndsGateImmediately() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 1U, 100U);
    fixture.begin();
    GateTrace trace;
    traceTicks(fixture.engine, trace, 1000U);
    CHECK_EQ(trace.rising[0].size(), 1U);
    CHECK(fixture.gates.channelStateHigh(0U));

    ChannelConfig off = fixture.state.channels[0];
    off.common.mode = ChannelMode::Off;
    traceTicks(fixture.engine, trace, 30000U, 1001U, [&] {
        fixture.engine.updateChannel(0U, off, true);
    });
    CHECK_EQ(trace.falling[0].size(), 1U);
    CHECK_EQ(trace.falling[0][0], 1001U);
    CHECK_EQ(trace.rising[0].size(), 1U);
    CHECK_EQ(static_cast<std::uint16_t>(trace.linesEverSet & gateLines({1U, 2U, 3U, 4U, 5U, 6U, 7U})), 0U);
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
}

void testTieKeepsGateHighAcrossStepsWithOneWritePerTick() {
    Fixture fixture;
    SequencerPatternV2 pattern{};
    pattern.length = 2U;
    pattern.gates = {{3ULL, 0ULL}};
    configureSequencerFixture(fixture, pattern);
    std::array<SequencerStepMetadataWord, kSequencerMaximumSteps> metadata{};
    SequencerStepMetadata first{};
    first.tie = true;
    metadata[0U] = packSequencerStepMetadata(first);
    fixture.engine.updateSequencerStepMetadata(0U, metadata);
    hal::platform::resetPortWriteLogForTest();

    GateTrace trace;
    traceTicks(fixture.engine, trace, 2900U);
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    CHECK_EQ(trace.rising[0].size(), 1U);
    CHECK_EQ(trace.falling[0].size(), 1U);
    CHECK(!fixture.gates.channelStateHigh(0U));
}

void testRatchetSubEventsKeepDistinctEdgesWithOneWritePerTick() {
    Fixture fixture;
    SequencerPatternV2 pattern{};
    pattern.length = 2U;
    pattern.gates = {{1ULL, 0ULL}};
    configureSequencerFixture(fixture, pattern);
    std::array<SequencerStepMetadataWord, kSequencerMaximumSteps> metadata{};
    SequencerStepMetadata step{};
    step.gateProfile = SequencerGateProfile::Duty75;
    step.ratchetCount = 4U;
    metadata[0U] = packSequencerStepMetadata(step);
    fixture.engine.updateSequencerStepMetadata(0U, metadata);

    GateTrace trace;
    // A tempo jump halfway through the ratchet group shortens the remaining
    // sub-intervals below the Duty 75 gate computed at the old tempo.
    traceTicks(fixture.engine, trace, 2500U, 400U, [&] { fixture.engine.updateMasterTempo(400U); });
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    CHECK(trace.rising[0].size() >= 4U);
    CHECK(edgesAlternateWithLowTick(trace, 0U));
    // The previous pulse ends one tick early; no rising edge leaves its tick.
    CHECK_EQ(fixture.engine.deferredGateRiseCountForTest(), 0U);
}

void testMutedOutputStageKeepsGatesLowWhileEngineRuns() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        configureClockChannel(fixture.state, channel, 2U, 10U);
    }
    fixture.begin();
    // Games and menus mute the stage; the engine keeps its timeline running.
    fixture.gates.disableOutputStage();
    GateTrace trace;
    traceTicks(fixture.engine, trace, 30000U);
    CHECK_EQ(trace.linesEverSet, 0U);
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK(!fixture.gates.channelStateHigh(channel));
        CHECK_EQ(fakefw::pinValues[pinmap::kGateChannelPins[channel]], LOW);
    }

    fixture.gates.enableOutputStage();
    GateTrace resumed;
    traceTicks(fixture.engine, resumed, 10000U);
    CHECK_EQ(resumed.linesEverSet, allGateLines());
}

void testGatesStayLowFromBootUntilPlay() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK_EQ(fakefw::pinValues[pinmap::kGateChannelPins[channel]], LOW);
        configureClockChannel(fixture.state, channel, 4U, 10U);
    }
    fixture.engine.begin(fixture.state);
    GateTrace trace;
    traceTicks(fixture.engine, trace, 20000U);
    CHECK_EQ(trace.linesEverSet, 0U);
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    fixture.engine.play();
    traceTicks(fixture.engine, trace, 2U);
    CHECK_EQ(trace.linesEverSet, allGateLines());
}

void testGateDriverWritesSeveralChannelsInOneAccess() {
    hal::GateOutputDriver gates;
    fakefw::resetArduino();
    gates.beginDisabled();
    gates.enableOutputStage();
    gates.setChannelStates(0xA5U);
    // Only changing lines are written: 0xA5 -> 0x5A raises four and lowers four in one access.
    hal::platform::resetPortWriteLogForTest();
    gates.setChannelStates(0x5AU);
    CHECK_EQ(hal::platform::portWriteCountForTest(), 1U);
    CHECK_EQ(hal::platform::lastPortWriteForTest().setMask, gateLines({1U, 3U, 4U, 6U}));
    CHECK_EQ(hal::platform::lastPortWriteForTest().resetMask, gateLines({0U, 2U, 5U, 7U}));
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK_EQ(gates.channelStateHigh(channel), ((0x5AU >> channel) & 1U) != 0U);
    }

    gates.disableOutputStage();
    gates.enableOutputStage();
    gates.setChannelStates(0xFFU);
    gates.disableOutputStage();
    hal::platform::resetPortWriteLogForTest();
    gates.setChannelStates(0xFFU);
    CHECK_EQ(hal::platform::portWriteCountForTest(), 1U);
    CHECK_EQ(hal::platform::lastPortWriteForTest().setMask, 0U);
    CHECK_EQ(hal::platform::lastPortWriteForTest().resetMask, 0U);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK(!gates.channelStateHigh(channel));
    }

    // Staging the same channel HIGH and then LOW leaves it only in the reset mask.
    gates.enableOutputStage();
    gates.stageChannelState(3U, true);
    gates.stageChannelState(3U, false);
    gates.stageChannelState(4U, false);
    gates.stageChannelState(4U, true);
    gates.stageChannelState(kChannelCount, true);
    hal::platform::resetPortWriteLogForTest();
    gates.commitStagedChannels();
    CHECK_EQ(hal::platform::portWriteCountForTest(), 1U);
    CHECK_EQ(hal::platform::lastPortWriteForTest().setMask, gateLines({4U}));
    CHECK_EQ(hal::platform::lastPortWriteForTest().resetMask, 0U);
    CHECK(gates.channelStateHigh(4U));
    CHECK(!gates.channelStateHigh(3U));
}

void testTempoJumpDuringHighGateKeepsEveryRisingEdge() {
    for (const std::uint16_t targetBpm : std::array<std::uint16_t, 2U>{{300U, 999U}}) {
        Fixture fixture;
        prepareInternalFixture(fixture);
        configureClockChannel(fixture.state, 0U, 1U, 240U);
        fixture.begin();
        GateTrace trace;
        // The 240 ms gate is HIGH from tick 1 to 4801; after the jump the next
        // beat arrives long before that.
        traceTicks(fixture.engine, trace, 40000U, 500U, [&] { fixture.engine.updateMasterTempo(targetBpm); });
        const std::uint32_t beats = fixture.engine.snapshot().masterBeatSerial;
        CHECK(beats > 4U);
        CHECK_EQ(trace.rising[0].size(), static_cast<std::size_t>(beats) + 1U);
        CHECK(edgesAlternateWithLowTick(trace, 0U));
        CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
        CHECK_EQ(fixture.engine.deferredGateRiseCountForTest(), 0U);
    }
}

void testLivePatternChangeDuringHighGateKeepsTheNextEdge() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 1U, 100U);
    fixture.state.channels[0].common.swingPercent = 50U;
    fixture.state.channels[0].common.groove = {GroovePreset::Custom, 100U, 0U, 0U};
    CustomGroovePattern straight{};
    straight.length = 2U;
    CustomGroovePattern opposed = straight;
    opposed.offsets256[0] = -120;
    opposed.offsets256[1] = 120;
    fixture.engine.updateCustomGrooveSlot(0U, straight, false);
    fixture.begin();
    GateTrace trace;
    // The swung odd event fires at 15000 with a 100 ms gate; 10 ms later the
    // opposed pattern pulls the next event to 15312, while the gate is HIGH.
    traceTicks(fixture.engine, trace, 40000U, 15200U, [&] {
        fixture.engine.updateCustomGrooveSlot(0U, opposed, true);
    });
    CHECK(hasRiseNear(trace, 0U, 15000U));
    CHECK(hasRiseNear(trace, 0U, 15312U));
    CHECK(edgesAlternateWithLowTick(trace, 0U));
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    CHECK_EQ(fixture.engine.deferredGateRiseCountForTest(), 0U);
}
