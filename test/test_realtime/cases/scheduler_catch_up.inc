/**
 * @file scheduler_catch_up.inc
 * @brief TIM3 updates merged during an interrupt lock are caught up from TIM5 (F-012).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * Each scheduler interrupt passes the microsecond timestamp of its TIM3 update. A
 * lock over k updates leaves one interrupt that must advance the timeline by k + 1
 * ticks. The reference is an engine that processed every tick on its own.
 */

/** @brief Runs one scheduler interrupt whose TIM3 update happened at @p updateUs. */
std::uint32_t schedulerIsrAt(Fixture& fixture, const std::uint32_t updateUs) {
    fixture.engine.beginSchedulerQuantumFromIsr();
    const std::uint32_t ticks = fixture.engine.elapsedSchedulerTicksFromIsr(updateUs);
    fixture.engine.processSchedulerTick(ticks);
    return ticks;
}

/** @brief Internal 120 BPM fixture with CLOCK x4 on channel 0, already playing. */
void prepareCatchUpFixture(Fixture& fixture) {
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 4U, 10U);
    fixture.begin();
}

/** @brief Master position after @p ticks individually processed ticks. */
std::uint64_t referencePositionAfterTicks(const std::uint32_t ticks) {
    Fixture reference;
    prepareCatchUpFixture(reference);
    runTicks(reference.engine, ticks);
    return reference.engine.snapshot().masterPositionQ32;
}

/**
 * @brief Runs @p beforeLock interrupts every 50 us, then one interrupt after a lock that
 *        merged @p lockedTicks updates, then @p afterLock regular interrupts.
 * @return Ticks reported by the interrupt after the lock.
 */
std::uint32_t runWithLock(
    Fixture& fixture,
    std::uint32_t& updateUs,
    const std::uint32_t beforeLock,
    const std::uint32_t lockedTicks,
    const std::uint32_t afterLock) {
    for (std::uint32_t index = 0U; index < beforeLock; ++index) {
        updateUs += config::kSchedulerTickUs;
        (void)schedulerIsrAt(fixture, updateUs);
    }
    updateUs += lockedTicks * config::kSchedulerTickUs;
    const std::uint32_t caughtUp = schedulerIsrAt(fixture, updateUs);
    for (std::uint32_t index = 0U; index < afterLock; ++index) {
        updateUs += config::kSchedulerTickUs;
        (void)schedulerIsrAt(fixture, updateUs);
    }
    return caughtUp;
}

void testLockOverOneTwoAndSevenUpdatesKeepsTheTimelineExact() {
    for (const std::uint32_t lockedTicks : {1U, 2U, 3U, 7U}) {
        Fixture fixture;
        prepareCatchUpFixture(fixture);
        std::uint32_t updateUs = 1'000'000U;
        // The first interrupt anchors the timeline and counts as one tick.
        (void)schedulerIsrAt(fixture, updateUs);
        const std::uint32_t caughtUp = runWithLock(fixture, updateUs, 999U, lockedTicks, 1000U);
        CHECK_EQ(caughtUp, lockedTicks);
        CHECK_EQ(fixture.engine.snapshot().masterPositionQ32,
                 referencePositionAfterTicks(1U + 999U + lockedTicks + 1000U));
        CHECK_EQ(fixture.engine.schedulerCatchUpLimitCountForTest(), 0U);
    }
}

void testCatchUpLimitExactlyBelowAndAbove() {
    constexpr std::uint32_t limit = config::kMaximumSchedulerCatchUpTicks;
    for (const std::uint32_t lockedTicks : {limit - 1U, limit, limit + 1U}) {
        Fixture fixture;
        prepareCatchUpFixture(fixture);
        std::uint32_t updateUs = 2'000'000U;
        (void)schedulerIsrAt(fixture, updateUs);
        const std::uint32_t caughtUp = runWithLock(fixture, updateUs, 100U, lockedTicks, 100U);
        const bool overLimit = lockedTicks > limit;
        // Above the limit the excess ticks are dropped; the TIM3 phase is kept.
        CHECK_EQ(caughtUp, overLimit ? limit : lockedTicks);
        CHECK_EQ(fixture.engine.schedulerCatchUpLimitCountForTest(), overLimit ? 1U : 0U);
        CHECK_EQ(fixture.engine.snapshot().masterPositionQ32,
                 referencePositionAfterTicks(1U + 100U + (overLimit ? limit : lockedTicks) + 100U));
    }
}

void testLockAcrossTheTim5WrapKeepsTheTimelineExact() {
    Fixture fixture;
    prepareCatchUpFixture(fixture);
    // Three updates before the 32-bit wrap, a lock over five updates across it.
    std::uint32_t updateUs = 0xFFFFFFFFU - 3U * config::kSchedulerTickUs - 20U;
    (void)schedulerIsrAt(fixture, updateUs);
    const std::uint32_t caughtUp = runWithLock(fixture, updateUs, 2U, 5U, 100U);
    CHECK(updateUs < 0x00010000U);
    CHECK_EQ(caughtUp, 5U);
    CHECK_EQ(fixture.engine.snapshot().masterPositionQ32,
             referencePositionAfterTicks(1U + 2U + 5U + 100U));
}

void testAlreadyCountedUpdateAdvancesNothing() {
    // A TIM3 update that re-enters right after the interrupt that already counted it.
    Fixture fixture;
    prepareCatchUpFixture(fixture);
    std::uint32_t updateUs = 500U;
    (void)schedulerIsrAt(fixture, updateUs);
    updateUs += 2U * config::kSchedulerTickUs;
    CHECK_EQ(schedulerIsrAt(fixture, updateUs), 2U);
    CHECK_EQ(schedulerIsrAt(fixture, updateUs), 0U);
    CHECK_EQ(fixture.engine.snapshot().masterPositionQ32, referencePositionAfterTicks(3U));
}

void testTempoStaysExactOverAMinuteOfRandomLocksAndJitter() {
    // 60 s at 120 BPM is exactly 120 beats, with random locks over 1..8 updates
    // and +-1 us timestamp quantization between TIM5 and TIM3.
    Fixture fixture;
    prepareCatchUpFixture(fixture);
    std::uint32_t random = 0x2545F491U;
    const auto nextRandom = [&random] {
        random = random * 1664525U + 1013904223U;
        return random >> 8U;
    };
    std::uint32_t tick = 0U;
    std::uint32_t isrCount = 0U;
    constexpr std::uint32_t kTicksPerMinute = 60U * config::kSchedulerFrequencyHz;
    while (tick < kTicksPerMinute) {
        std::uint32_t step = 1U;
        if (tick != 0U && nextRandom() % 16U == 0U) {
            step += nextRandom() % config::kMaximumSchedulerCatchUpTicks;
        }
        step = std::min(step, kTicksPerMinute - tick);
        tick += step;
        const std::uint32_t jitterUs = nextRandom() % 3U;
        (void)schedulerIsrAt(fixture, 3'000'000U + tick * config::kSchedulerTickUs + jitterUs - 1U);
        ++isrCount;
    }
    const engine::EngineSnapshot snapshot = fixture.engine.snapshot();
    // The first interrupt counts one tick for the anchor, which tick 1 represents.
    CHECK(isrCount < kTicksPerMinute);
    CHECK_EQ(snapshot.masterPositionQ32, 120ULL * core::kQ32One);
    CHECK_EQ(snapshot.masterBeatSerial, 120U);
    CHECK_EQ(fixture.engine.schedulerCatchUpLimitCountForTest(), 0U);
}

void testEventsInCaughtUpTicksFireOnceAndInOrder() {
    // CLOCK x4 fires every 2500 ticks. Locks over 7 updates around every event
    // must still produce one rising edge per event, none twice, none lost.
    Fixture fixture;
    prepareCatchUpFixture(fixture);
    configureClockChannel(fixture.state, 1U, 1U, 10U);
    fixture.state.channels[1].clock.meter = {4U, 4U};
    fixture.engine.updateConfiguration(fixture.state, true);
    GateTrace trace;
    std::uint32_t updateUs = 10'000U;
    std::uint32_t tick = 0U;
    while (tick < 39000U) {
        // Lock from 3 ticks before each x4 event to 4 ticks after it.
        const bool lock = (tick + 3U) % 2500U == 0U;
        const std::uint32_t step = lock ? 7U : 1U;
        tick += step;
        updateUs += step * config::kSchedulerTickUs;
        fakefw::writes.clear();
        (void)schedulerIsrAt(fixture, updateUs);
        recordGateWrites(trace, tick);
    }
    CHECK_EQ(trace.rising[0].size(), 16U);
    CHECK_EQ(trace.rising[1].size(), 4U);
    CHECK(edgesAlternateWithLowTick(trace, 0U));
    // Each x4 event is caught up in the interrupt that ends its lock (tick 2500k + 4).
    for (std::uint32_t index = 1U; index < trace.rising[0].size(); ++index) {
        CHECK_EQ(trace.rising[0][index], index * 2500U + 4U);
    }
    // An event inside a lock advances the channel step exactly once.
    Fixture dense;
    prepareInternalFixture(dense);
    configureClockChannel(dense.state, 0U, 16U, 1U);
    dense.begin();
    std::uint32_t denseUs = 0U;
    (void)schedulerIsrAt(dense, denseUs);
    const std::uint8_t stepBefore = dense.engine.snapshot().channelStep[0];
    // x16 at 120 BPM: one event every 625 ticks; a lock over 8 updates around
    // tick 625 holds one event.
    for (std::uint32_t index = 0U; index < 620U; ++index) {
        denseUs += config::kSchedulerTickUs;
        (void)schedulerIsrAt(dense, denseUs);
    }
    denseUs += 8U * config::kSchedulerTickUs;
    CHECK_EQ(schedulerIsrAt(dense, denseUs), 8U);
    CHECK_EQ(static_cast<std::uint8_t>(dense.engine.snapshot().channelStep[0] - stepBefore), 1U);
}

void testGateLengthCountsCaughtUpTicks() {
    // A 10 ms gate (200 ticks) ends 200 ticks after its rise, locks included.
    Fixture fixture;
    prepareInternalFixture(fixture);
    configureClockChannel(fixture.state, 0U, 1U, 10U);
    fixture.begin();
    GateTrace trace;
    std::uint32_t updateUs = 0U;
    std::uint32_t tick = 0U;
    while (tick < 1000U) {
        const std::uint32_t step = tick >= 50U && tick < 150U ? 5U : 1U;
        tick += step;
        updateUs += step * config::kSchedulerTickUs;
        fakefw::writes.clear();
        (void)schedulerIsrAt(fixture, updateUs);
        recordGateWrites(trace, tick);
    }
    CHECK_EQ(trace.rising[0].size(), 1U);
    CHECK_EQ(trace.falling[0].size(), 1U);
    CHECK(trace.falling[0][0] >= 200U && trace.falling[0][0] <= 205U);
}
