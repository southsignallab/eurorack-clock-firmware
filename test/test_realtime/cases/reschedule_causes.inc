/**
 * @file reschedule_causes.inc
 * @brief Every live reschedule cause emits each event once and never re-fires the last one (F-063).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * 120 BPM in 4/4: one beat is 10000 scheduler ticks. CLOCK x1 serial n fires in
 * tick n * 10000 (serial 0 in tick 1). Each case changes the channel 100 ticks
 * after serial 1 (gate 10 ms, already LOW) and checks the ticks of the following
 * rising edges against the hand-calculated grid.
 */

/** @brief Runs a CLOCK channel, applies @p change before @p changeTick and returns its rising-edge trace. */
template <typename Configure, typename Change>
GateTrace traceReschedule(
    Configure configure,
    const std::uint32_t changeTick,
    const std::uint32_t ticks,
    Change change) {
    Fixture fixture;
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 1U, 10U);
    configure(fixture);
    fixture.begin();
    GateTrace trace;
    traceTicks(fixture.engine, trace, ticks, changeTick, [&] { change(fixture); });
    CHECK(edgesAlternateWithLowTick(trace, 0U));
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    return trace;
}

/** @brief Asserts the rises after @p fromTick are exactly @p expected (each within +-2 ticks). */
void checkRisesAfter(
    const GateTrace& trace,
    const std::uint32_t fromTick,
    const std::initializer_list<std::uint32_t> expected) {
    CHECK_EQ(countRisesBetween(trace, 0U, fromTick + 1U, trace.ticks), expected.size());
    for (const std::uint32_t tick : expected) {
        CHECK(hasRiseNear(trace, 0U, tick));
    }
}

void installStraightAndOpposed(Fixture& fixture, CustomGroovePattern& opposed) {
    CustomGroovePattern straight{};
    straight.length = 2U;
    opposed = straight;
    opposed.offsets256[0] = -120;
    opposed.offsets256[1] = 120;
    fixture.state.channels[0].common.groove = {GroovePreset::Custom, 100U, 0U, 0U};
    fixture.engine.updateCustomGrooveSlot(0U, straight, false);
}

void testCustomGrooveChangeDoesNotRefireLastEvent() {
    CustomGroovePattern opposed{};
    // [-120, +120]: serial 1 moves to 14687 (already played), serial 2 to 15312.
    const GateTrace trace = traceReschedule(
        [&](Fixture& fixture) { installStraightAndOpposed(fixture, opposed); },
        13000U, 40000U,
        [&](Fixture& fixture) { fixture.engine.updateCustomGrooveSlot(0U, opposed, true); });
    CHECK_EQ(countRisesBetween(trace, 0U, 1U, 12999U), 2U);
    checkRisesAfter(trace, 13000U, {15312U, 34687U, 35312U});
}

void testCustomGroovePreviewDoesNotRefireLastEvent() {
    CustomGroovePattern opposed{};
    const GateTrace trace = traceReschedule(
        [&](Fixture& fixture) { installStraightAndOpposed(fixture, opposed); },
        13000U, 40000U,
        [&](Fixture& fixture) { fixture.engine.setCustomGroovePreview(0U, opposed, 100U, 0U, true); });
    checkRisesAfter(trace, 13000U, {15312U, 34687U, 35312U});
}

void testSwingIncreaseDoesNotRefireLastEvent() {
    // CLOCK x2: serial 1 at 5000; Swing 50 moves it to 7500, serial 3 to 17500.
    const GateTrace trace = traceReschedule(
        [](Fixture& fixture) { fixture.state.channels[0].common.rate = {ClockRatioMode::Multiply, 2U, 1U, 1U}; },
        5100U, 20000U,
        [](Fixture& fixture) {
            fixture.state.channels[0].common.swingPercent = 50U;
            fixture.engine.updateChannel(0U, fixture.state.channels[0], true);
        });
    checkRisesAfter(trace, 5100U, {10000U, 17500U, 20000U});
}

void testGroovePresetChangeDoesNotRefireLastEvent() {
    const GateTrace trace = traceReschedule(
        [](Fixture& fixture) { fixture.state.channels[0].common.rate = {ClockRatioMode::Multiply, 2U, 1U, 1U}; },
        5100U, 9999U,
        [](Fixture& fixture) {
            fixture.state.channels[0].common.groove = {GroovePreset::Swing66, 100U, 0U, 0U};
            fixture.engine.updateConfiguration(fixture.state, true);
        });
    CHECK_EQ(countRisesBetween(trace, 0U, 5101U, 9999U), 0U);
}

void testGrooveRotationDoesNotRefireLastEvent() {
    CustomGroovePattern leading{};
    leading.length = 2U;
    leading.offsets256[0] = 120;
    // Rotation 1 gives serial 1 the +120 offset (14687, already played); serial 2 stays at 20000.
    const GateTrace trace = traceReschedule(
        [&](Fixture& fixture) {
            fixture.state.channels[0].common.groove = {GroovePreset::Custom, 100U, 0U, 0U};
            fixture.engine.updateCustomGrooveSlot(0U, leading, false);
        },
        10100U, 20100U,
        [](Fixture& fixture) {
            fixture.state.channels[0].common.groove.rotation = 1U;
            fixture.engine.updateChannel(0U, fixture.state.channels[0], true);
        });
    checkRisesAfter(trace, 10100U, {20000U});
}

void testPhaseChangeDoesNotRefireLastEvent() {
    // Phase 25 % moves serial 1 to 12500 (already played) and serial 2 to 22500.
    const GateTrace trace = traceReschedule(
        [](Fixture&) {}, 10100U, 22600U,
        [](Fixture& fixture) {
            fixture.state.channels[0].common.phasePercent = 25U;
            fixture.engine.updateChannel(0U, fixture.state.channels[0], true);
        });
    checkRisesAfter(trace, 10100U, {22500U});
}

void testRateChangeKeepsTheNextGridEvent() {
    // x4 -> x1 after serial 4 (tick 10000): the next x1 event is beat 2 at 20000.
    const GateTrace slower = traceReschedule(
        [](Fixture& fixture) { fixture.state.channels[0].common.rate = {ClockRatioMode::Multiply, 4U, 1U, 1U}; },
        10100U, 20100U,
        [](Fixture& fixture) {
            fixture.state.channels[0].common.rate = {ClockRatioMode::Multiply, 1U, 1U, 1U};
            fixture.engine.updateChannel(0U, fixture.state.channels[0], true);
        });
    checkRisesAfter(slower, 10100U, {20000U});
    // x1 -> x4 after serial 1: beat 1 is not repeated, the next sixteenth is 12500.
    const GateTrace faster = traceReschedule(
        [](Fixture&) {}, 10100U, 15100U,
        [](Fixture& fixture) {
            fixture.state.channels[0].common.rate = {ClockRatioMode::Multiply, 4U, 1U, 1U};
            fixture.engine.updateChannel(0U, fixture.state.channels[0], true);
        });
    checkRisesAfter(faster, 10100U, {12500U, 15000U});
}

void testModeAndPatternChangesKeepTheNextGridEvent() {
    const GateTrace modeChange = traceReschedule(
        [](Fixture&) {}, 10100U, 20100U,
        [](Fixture& fixture) {
            fixture.state.channels[0].common.mode = ChannelMode::Euclid;
            fixture.state.channels[0].euclid = {4U, 4U, 0U};
            fixture.engine.updateChannel(0U, fixture.state.channels[0], true);
        });
    // EUCLID x1 runs on sixteenths: beat 1 (10000) is not repeated, 12500 is next.
    checkRisesAfter(modeChange, 10100U, {12500U, 15000U, 17500U, 20000U});

    const GateTrace patternChange = traceReschedule(
        [](Fixture& fixture) {
            fixture.state.channels[0].common.mode = ChannelMode::Sequencer;
            SequencerPatternV2 pattern{};
            pattern.length = 4U;
            pattern.gates = {{0xFULL, 0ULL}};
            fixture.engine.updateSequencerPattern(0U, pattern, false);
        },
        10100U, 20100U,
        [](Fixture& fixture) {
            SequencerPatternV2 pattern{};
            pattern.length = 4U;
            pattern.gates = {{0xFULL, 0ULL}};
            pattern.rotation = 1U;
            fixture.engine.updateSequencerPattern(0U, pattern, true);
        });
    // SEQ x1 runs on sixteenths; a rotated pattern keeps the grid.
    checkRisesAfter(patternChange, 10100U, {12500U, 15000U, 17500U, 20000U});
}

void testTempoChangeDoesNotRefireLastEvent() {
    // 60 BPM: the remaining 0.99 beat to beat 2 takes 19800 ticks.
    const GateTrace trace = traceReschedule(
        [](Fixture&) {}, 10100U, 30100U,
        [](Fixture& fixture) { fixture.engine.updateMasterTempo(60U); });
    checkRisesAfter(trace, 10100U, {29900U});
}

void testExternalSyncCorrectionDoesNotRefireOrDropEvents() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    fixture.state.source = ClockSource::External;
    configureClockChannel(fixture.state, 0U, 1U, 10U);
    fixture.engine.begin(fixture.state);
    fixture.engine.setExternalLock(true, 120000U);
    fixture.engine.play();
    GateTrace trace;
    // First pulse sets the phase; then one beat per pulse, one late and one early.
    traceTicks(fixture.engine, trace, 100U, 100U, [&] { fixture.engine.acceptExternalPulse(120000U, 1U); });
    for (const std::uint32_t pulseTick : {10100U, 20400U, 30100U, 39800U, 49800U}) {
        traceTicks(fixture.engine, trace, pulseTick - trace.ticks, pulseTick,
                   [&] { fixture.engine.acceptExternalPulse(120000U, 1U); });
    }
    traceTicks(fixture.engine, trace, 2000U);
    CHECK(edgesAlternateWithLowTick(trace, 0U));
    // Locked PLAY fires beat 1 in tick 1; the phase pulse restarts the bar while
    // that gate is HIGH, so its rise moves to tick 101. Then one edge per beat:
    // late and early pulses shift the grid but neither repeat nor drop a beat.
    CHECK_EQ(trace.rising[0].size(), 7U);
    CHECK(hasRiseNear(trace, 0U, 101U));
    CHECK(std::find(trace.falling[0].begin(), trace.falling[0].end(), 100U) != trace.falling[0].end());
    CHECK(hasRiseNear(trace, 0U, 30100U));
    CHECK(hasRiseNear(trace, 0U, 39800U));
}

void testResetRestartsTheBarExactlyOnce() {
    // A reset re-plays beat 1 on purpose; it fires once, not twice.
    const GateTrace trace = traceReschedule(
        [](Fixture&) {}, 10100U, 19000U,
        [](Fixture& fixture) { fixture.engine.resetGlobalPhase(); });
    CHECK_EQ(countRisesBetween(trace, 0U, 10100U, 19000U), 1U);
    CHECK(hasRiseNear(trace, 0U, 10100U));
}

void testRescheduleInSameTickAsHighGateMovesRiseByOneTick() {
    Fixture fixture;
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 1U, 10U);
    fixture.begin();
    GateTrace trace;
    // The 10 ms gate of serial 1 is HIGH from 10000 to 10200. A reset before tick
    // 10050 restarts the bar at once: the new rise cannot share the falling write.
    traceTicks(fixture.engine, trace, 1000U, 10050U, [&] { fixture.engine.resetGlobalPhase(); });
    traceTicks(fixture.engine, trace, 10000U, 10050U, [&] { fixture.engine.resetGlobalPhase(); });
    CHECK_EQ(fixture.engine.deferredGateRiseCountForTest(), 1U);
    CHECK(std::find(trace.falling[0].begin(), trace.falling[0].end(), 10050U) != trace.falling[0].end());
    CHECK(std::find(trace.rising[0].begin(), trace.rising[0].end(), 10051U) != trace.rising[0].end());
    CHECK(edgesAlternateWithLowTick(trace, 0U));
}
