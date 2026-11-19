/**
 * @file merged_edges.inc
 * @brief Included SYNC behavior cases: edges merged into one EXTI interrupt (F-016, D-15).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * The EXTI pending bit keeps one request for any number of edges. When the callback
 * reads the same level as the last stored one, at least two edges merged; the capture
 * then queues the missing opposite edge and the observed edge with one timestamp.
 */

/** @brief Starts the real capture path with INPUT 1 and INPUT 2 resting at @p level. */
void beginCaptureAt(Fixture& fixture, const std::uint8_t level) {
    fakefw::setPinSilently(pinmap::kExternalSyncSignalPin, level);
    fakefw::setPinSilently(pinmap::kExternalResetSignalPin, level);
    fixture.inputs.begin();
}

/** @brief One EXTI interrupt after the pin changed and returned: the level is unchanged. */
void mergedPulseAt(const std::uint32_t pin, const std::uint32_t nowUs) {
    fakefw::nowUs = nowUs;
    fakefw::interruptBindings[pin].callback();
}

/** @brief One EXTI interrupt for a single edge to @p level. */
void edgeAt(const std::uint32_t pin, const std::uint8_t level, const std::uint32_t nowUs) {
    fakefw::nowUs = nowUs;
    fakefw::setPin(pin, level);
}

void testShortStartPulseBetweenLowLevelsStartsOnce() {
    Fixture fixture;
    fixture.state.inputs = {InputFunction::Start, InputFunction::Off};
    fixture.engine.begin(fixture.state);
    fixture.sync.begin(fixture.state);
    fixture.engine.stop();
    beginCaptureAt(fixture, LOW);

    // LOW before and after: the rise and the fall merged into one interrupt.
    mergedPulseAt(pinmap::kExternalSyncSignalPin, 5000U);
    fixture.sync.processSchedulerTick(5050U);

    CHECK(fixture.engine.isPlaying());
    TransportState transition{};
    CHECK(fixture.sync.consumeTransportTransition(transition));
    CHECK_EQ(transition, TransportState::Playing);
    CHECK(!fixture.sync.consumeTransportTransition(transition));
    CHECK_EQ(fixture.inputs.synthesizedEdgesForTest(), 1U);
    CHECK(!fixture.inputs.syncLevelHigh());
}

void testMergedEdgesBetweenHighLevelsQueueFallThenRise() {
    Fixture fixture;
    fixture.state.inputs = {InputFunction::Off, InputFunction::Off};
    fixture.engine.begin(fixture.state);
    fixture.sync.begin(fixture.state);
    beginCaptureAt(fixture, HIGH);

    mergedPulseAt(pinmap::kExternalSyncSignalPin, 7000U);

    hal::ExternalInputEdge edge{};
    CHECK(fixture.inputs.popSyncEdge(edge));
    CHECK(!edge.high);
    CHECK_EQ(edge.timestampUs, 7000U);
    CHECK(!edge.continuityLost);
    CHECK(fixture.inputs.popSyncEdge(edge));
    CHECK(edge.high);
    CHECK_EQ(edge.timestampUs, 7000U);
    CHECK(!edge.continuityLost);
    CHECK(!fixture.inputs.popSyncEdge(edge));
    CHECK(fixture.inputs.syncLevelHigh());
}

void testLongStartPulseStartsOnceWithoutSynthesizedEdges() {
    Fixture fixture;
    fixture.state.inputs = {InputFunction::Start, InputFunction::Off};
    fixture.engine.begin(fixture.state);
    fixture.sync.begin(fixture.state);
    fixture.engine.stop();
    beginCaptureAt(fixture, LOW);

    edgeAt(pinmap::kExternalSyncSignalPin, HIGH, 5000U);
    fixture.sync.processSchedulerTick(5050U);
    edgeAt(pinmap::kExternalSyncSignalPin, LOW, 15000U);
    fixture.sync.processSchedulerTick(15050U);

    CHECK(fixture.engine.isPlaying());
    TransportState transition{};
    CHECK(fixture.sync.consumeTransportTransition(transition));
    CHECK(!fixture.sync.consumeTransportTransition(transition));
    CHECK_EQ(fixture.inputs.synthesizedEdgesForTest(), 0U);
}

void testMergedSyncPulseWithRepeatedTimestampKeepsTempo() {
    Fixture fixture;
    fixture.engine.begin(fixture.state);
    fixture.sync.begin(fixture.state);
    fixture.engine.play();
    beginCaptureAt(fixture, LOW);

    // Rising edges every 500 ms (120 BPM).
    for (std::uint32_t pulse = 0U; pulse < 4U; ++pulse) {
        const std::uint32_t risingUs = 1000U + pulse * 500000U;
        edgeAt(pinmap::kExternalSyncSignalPin, HIGH, risingUs);
        fixture.sync.processSchedulerTick(risingUs);
        edgeAt(pinmap::kExternalSyncSignalPin, LOW, risingUs + 1000U);
        fixture.sync.processSchedulerTick(risingUs + 1000U);
    }
    CHECK(fixture.engine.snapshot().externalLocked);
    const std::uint32_t lockedBpm = fixture.sync.filteredBpmMilli();
    CHECK_NEAR(lockedBpm, 120000U, 1U);

    // Rise at 2 001 000 us, then a fall and a rise merged into one interrupt in the
    // same microsecond: the synthesized fall and the observed rise share it. The
    // second rise has period 0 and is rejected; no division by zero, no tempo step.
    const std::uint32_t lastRiseUs = 1000U + 4U * 500000U;
    edgeAt(pinmap::kExternalSyncSignalPin, HIGH, lastRiseUs);
    mergedPulseAt(pinmap::kExternalSyncSignalPin, lastRiseUs);
    fixture.sync.processSchedulerTick(lastRiseUs);
    CHECK_EQ(fixture.inputs.synthesizedEdgesForTest(), 1U);
    CHECK(fixture.engine.snapshot().externalLocked);
    CHECK_EQ(fixture.sync.filteredBpmMilli(), lockedBpm);

    // The next regular rise measures from the accepted rise, not from the duplicate.
    edgeAt(pinmap::kExternalSyncSignalPin, LOW, lastRiseUs + 1000U);
    edgeAt(pinmap::kExternalSyncSignalPin, HIGH, lastRiseUs + 500000U);
    fixture.sync.processSchedulerTick(lastRiseUs + 500000U);
    CHECK_NEAR(fixture.sync.filteredBpmMilli(), 120000U, 1U);
}

void testFullQueueKeepsContinuityMarkerWithSynthesizedEdge() {
    for (const std::uint32_t queued : {14U, 15U}) {
        Fixture fixture;
        fixture.state.inputs = {InputFunction::Off, InputFunction::Off};
        fixture.engine.begin(fixture.state);
        fixture.sync.begin(fixture.state);
        beginCaptureAt(fixture, LOW);

        // Fill the 16-slot ring (15 usable) with alternating single edges, ending LOW.
        std::uint8_t level = LOW;
        for (std::uint32_t index = 0U; index < queued; ++index) {
            level = level == LOW ? HIGH : LOW;
            edgeAt(pinmap::kExternalSyncSignalPin, level, 1000U + index);
        }
        const bool levelBefore = level == HIGH;
        // Merged pulse: two more edges at one timestamp; at least one does not fit.
        mergedPulseAt(pinmap::kExternalSyncSignalPin, 2000U);

        hal::ExternalInputEdge edge{};
        std::uint32_t popped = 0U;
        std::uint32_t markers = 0U;
        bool markerLast = false;
        while (fixture.inputs.popSyncEdge(edge)) {
            ++popped;
            markerLast = edge.continuityLost;
            if (edge.continuityLost) {
                ++markers;
                CHECK_EQ(edge.timestampUs, 2000U);
            }
        }
        // 14 queued: the synthesized edge fits, the observed one becomes the marker.
        // 15 queued: the synthesized edge becomes the marker, the observed one is dropped.
        CHECK_EQ(markers, 1U);
        CHECK(markerLast);
        CHECK_EQ(popped, 16U);
        CHECK_EQ(fixture.inputs.syncLevelHigh(), levelBefore);
        CHECK_EQ(fixture.inputs.droppedSyncEdges(), queued == 14U ? 1U : 2U);
        CHECK_EQ(fixture.inputs.synthesizedEdgesForTest(), 1U);
    }
}

void testRunGateFollowsLevelThroughMergedEdges() {
    Fixture fixture;
    fixture.state.inputs = {InputFunction::Run, InputFunction::Off};
    fakefw::setPinSilently(pinmap::kExternalSyncSignalPin, LOW);
    fakefw::setPinSilently(pinmap::kExternalResetSignalPin, LOW);
    fixture.inputs.begin();
    fixture.engine.begin(fixture.state);
    fixture.sync.begin(fixture.state);
    fixture.engine.stop();
    fixture.sync.processSchedulerTick(1000U);
    CHECK(!fixture.engine.isPlaying());

    // LOW, short HIGH, LOW again in one interrupt: RUN stays stopped.
    mergedPulseAt(pinmap::kExternalSyncSignalPin, 2000U);
    fixture.sync.processSchedulerTick(2050U);
    CHECK(!fixture.engine.isPlaying());
    TransportState transition{};
    CHECK(!fixture.sync.consumeTransportTransition(transition));

    // HIGH starts; a short LOW gap merged into one interrupt keeps it running.
    edgeAt(pinmap::kExternalSyncSignalPin, HIGH, 3000U);
    fixture.sync.processSchedulerTick(3050U);
    CHECK(fixture.engine.isPlaying());
    CHECK(fixture.sync.consumeTransportTransition(transition));
    mergedPulseAt(pinmap::kExternalSyncSignalPin, 4000U);
    fixture.sync.processSchedulerTick(4050U);
    CHECK(fixture.engine.isPlaying());
    CHECK(!fixture.sync.consumeTransportTransition(transition));
}

void testResetRolesHandleMergedEdges() {
    // RESET as trigger: one reset for a merged pulse.
    {
        Fixture fixture;
        fixture.state.inputs = {InputFunction::Off, InputFunction::Reset};
        fixture.state.externalSync.resetMode = ExternalResetMode::Trigger;
        fixture.engine.begin(fixture.state);
        fixture.sync.begin(fixture.state);
        beginCaptureAt(fixture, LOW);
        fixture.sync.processSchedulerTick(1000U);
        const std::uint32_t before = fixture.engine.resetRuntimeCountForTest();
        mergedPulseAt(pinmap::kExternalResetSignalPin, 2000U);
        fixture.sync.processSchedulerTick(2050U);
        CHECK_EQ(fixture.engine.resetRuntimeCountForTest(), before + 1U);
    }
    // RESET as gate: only the level counts; the hold stays released.
    {
        Fixture fixture;
        fixture.state.inputs = {InputFunction::Off, InputFunction::Reset};
        fixture.state.externalSync.resetMode = ExternalResetMode::Gate;
        fixture.engine.begin(fixture.state);
        fixture.sync.begin(fixture.state);
        beginCaptureAt(fixture, LOW);
        fixture.sync.processSchedulerTick(1000U);
        mergedPulseAt(pinmap::kExternalResetSignalPin, 2000U);
        fixture.sync.processSchedulerTick(2050U);
        CHECK(!fixture.engine.snapshot().externalResetHeld);
    }
}

void testMergedPulsesOnBothInputsInOneTick() {
    Fixture fixture;
    fixture.state.inputs = {InputFunction::Start, InputFunction::Reset};
    fixture.state.externalSync.resetMode = ExternalResetMode::Trigger;
    fixture.engine.begin(fixture.state);
    fixture.sync.begin(fixture.state);
    fixture.engine.stop();
    beginCaptureAt(fixture, LOW);
    fixture.sync.processSchedulerTick(1000U);
    const std::uint32_t resetsBefore = fixture.engine.resetRuntimeCountForTest();

    mergedPulseAt(pinmap::kExternalSyncSignalPin, 2000U);
    mergedPulseAt(pinmap::kExternalResetSignalPin, 2010U);
    fixture.sync.processSchedulerTick(2050U);

    CHECK(fixture.engine.isPlaying());
    TransportState transition{};
    CHECK(fixture.sync.consumeTransportTransition(transition));
    CHECK_EQ(transition, TransportState::Playing);
    CHECK(!fixture.sync.consumeTransportTransition(transition));
    // One reset from RESET, one restart from START.
    CHECK_EQ(fixture.engine.resetRuntimeCountForTest(), resetsBefore + 2U);
    CHECK_EQ(fixture.inputs.synthesizedEdgesForTest(), 2U);
}
