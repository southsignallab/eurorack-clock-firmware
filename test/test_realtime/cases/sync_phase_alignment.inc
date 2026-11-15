/**
 * @file sync_phase_alignment.inc
 * @brief External sync phase corrections keep the GLOBAL grid on the pulses (F-069).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * 120 BPM, 4/4, one pulse per quarter note: one beat is 10000 scheduler ticks.
 * A pulse handed to the engine before tick p is observed at the master position
 * of tick p - 1. The first pulse sets the phase; every later pulse snaps the
 * GLOBAL grid so that a beat lies on it. Early pulses move the grid back, late
 * pulses forward; neither may leave a residual offset on the following beats.
 */

/** @brief Locked EXTERNAL fixture with CLOCK x1 on channel 0 (GLOBAL) and channel 1 (FREE). */
void prepareLockedSyncFixture(Fixture& fixture) {
    prepareInternalFixture(fixture);
    fixture.state.source = ClockSource::External;
    configureClockChannel(fixture.state, 0U, 1U, 10U);
    configureClockChannel(fixture.state, 1U, 1U, 10U);
    fixture.state.channels[1].common.resetMode = ResetMode::Free;
    fixture.engine.begin(fixture.state);
    fixture.engine.setExternalLock(true, 120000U);
    fixture.engine.play();
}

/** @brief Feeds one pulse before each listed tick, then runs @p tailTicks more ticks. */
GateTrace traceSyncPulses(
    Fixture& fixture,
    const std::initializer_list<std::uint32_t> pulseTicks,
    const std::uint32_t tailTicks) {
    GateTrace trace;
    for (const std::uint32_t pulseTick : pulseTicks) {
        traceTicks(fixture.engine, trace, pulseTick - trace.ticks, pulseTick,
                   [&] { fixture.engine.acceptExternalPulse(120000U, 1U); });
    }
    traceTicks(fixture.engine, trace, tailTicks);
    CHECK(edgesAlternateWithLowTick(trace, 0U));
    CHECK(edgesAlternateWithLowTick(trace, 1U));
    CHECK_EQ(trace.ticksWithoutSinglePortWrite, 0U);
    return trace;
}

void testOnTimePulseAfterEarlyPulsesFiresTheBeatOnThePulse() {
    // B-08 probe: late (20400), early (30100, 39800), then on time (49800). The
    // early corrections exceed the epoch set by the phase pulse (tick 99).
    Fixture fixture;
    prepareLockedSyncFixture(fixture);
    const GateTrace trace = traceSyncPulses(
        fixture, {100U, 10100U, 20400U, 30100U, 39800U, 49800U, 59800U}, 2000U);
    CHECK(hasRiseNear(trace, 0U, 30100U));
    CHECK(hasRiseNear(trace, 0U, 39800U));
    CHECK(hasRiseNear(trace, 0U, 49800U));
    CHECK(hasRiseNear(trace, 0U, 59800U));
    CHECK_EQ(countRisesBetween(trace, 0U, 49803U, 59797U), 0U);
}

void testEarlyPulseBeyondTheEpochKeepsTheFollowingBeats() {
    // One early pulse (500 ticks) right after the phase pulse, then on time again.
    Fixture fixture;
    prepareLockedSyncFixture(fixture);
    const GateTrace trace = traceSyncPulses(fixture, {100U, 9600U, 19600U, 29600U}, 2000U);
    CHECK(hasRiseNear(trace, 0U, 9600U));
    CHECK(hasRiseNear(trace, 0U, 19600U));
    CHECK(hasRiseNear(trace, 0U, 29600U));
    CHECK_EQ(countRisesBetween(trace, 0U, 9603U, 19597U), 0U);
}

void testLatePulseMovesTheGridWithoutRepeatingTheBeat() {
    Fixture fixture;
    prepareLockedSyncFixture(fixture);
    const GateTrace trace = traceSyncPulses(fixture, {100U, 10400U, 20400U, 30400U}, 2000U);
    // The beat at 10100 already played; the late pulse does not repeat it.
    CHECK(hasRiseNear(trace, 0U, 10100U));
    CHECK_EQ(countRisesBetween(trace, 0U, 10103U, 20397U), 0U);
    CHECK(hasRiseNear(trace, 0U, 20400U));
    CHECK(hasRiseNear(trace, 0U, 30400U));
}

void testOnTimePulsesKeepTheGridUnchanged() {
    Fixture fixture;
    prepareLockedSyncFixture(fixture);
    const GateTrace trace = traceSyncPulses(fixture, {100U, 10100U, 20100U, 30100U}, 2000U);
    CHECK(hasRiseNear(trace, 0U, 10100U));
    CHECK(hasRiseNear(trace, 0U, 20100U));
    CHECK(hasRiseNear(trace, 0U, 30100U));
    CHECK_EQ(trace.rising[0].size(), 5U);
}

void testOutlierPulseRealignsOnceAndTheGridFollowsTheNextPulses() {
    // Half a beat early, then the pulses continue on the new phase.
    Fixture fixture;
    prepareLockedSyncFixture(fixture);
    const GateTrace trace = traceSyncPulses(fixture, {100U, 10100U, 15100U, 25100U, 35100U}, 2000U);
    CHECK(hasRiseNear(trace, 0U, 10100U));
    CHECK(hasRiseNear(trace, 0U, 15100U));
    CHECK(hasRiseNear(trace, 0U, 25100U));
    CHECK(hasRiseNear(trace, 0U, 35100U));
    CHECK_EQ(countRisesBetween(trace, 0U, 15103U, 25097U), 0U);
}

void testFreeChannelIgnoresEarlyAndLatePulseCorrections() {
    // FREE keeps its own grid: one beat every 10000 ticks from its start.
    Fixture fixture;
    prepareLockedSyncFixture(fixture);
    const GateTrace trace = traceSyncPulses(fixture, {100U, 9600U, 20400U, 29600U}, 2000U);
    for (const std::uint32_t tick : {10001U, 20001U, 30001U}) {
        CHECK(hasRiseNear(trace, 1U, tick));
    }
}
