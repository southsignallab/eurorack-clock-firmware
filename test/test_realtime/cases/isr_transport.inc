/**
 * @file isr_transport.inc
 * @brief Included real-time cases: transport from interrupt context without locks (F-030, F-075).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * The scheduler interrupt has the highest priority; nothing it races with can run
 * while it runs, and an InterruptLock there only hides nesting (interrupt_lock.h).
 * Every case counts the locks opened inside one scheduler tick.
 */

/** @brief Runs one scheduler tick and returns how many InterruptLocks it opened. */
std::uint32_t locksInTick(Fixture& fixture, const std::uint32_t nowUs) {
    const std::uint32_t before = hal::InterruptLock::constructionCountForTest();
    fixture.tick(nowUs);
    return hal::InterruptLock::constructionCountForTest() - before;
}

/** @brief Queues a rising and a falling edge on INPUT 1 at @p risingUs. */
void pulseOnInput1(Fixture& fixture, const std::uint32_t risingUs) {
    fixture.inputs.injectSyncEdgeForTest(risingUs, true);
    fixture.inputs.injectSyncEdgeForTest(risingUs + 1000U, false);
}

void testInputTransportRolesOpenNoLockInTheSchedulerTick() {
    // START from STOP, then STOP.
    {
        Fixture fixture;
        fixture.state.inputs = {InputFunction::Start, InputFunction::Stop};
        fixture.begin();
        fixture.engine.stop();
        pulseOnInput1(fixture, 1000U);
        CHECK_EQ(locksInTick(fixture, 2100U), 0U);
        CHECK(fixture.engine.isPlaying());
        fixture.inputs.injectResetEdgeForTest(3000U, true);
        CHECK_EQ(locksInTick(fixture, 3050U), 0U);
        CHECK(!fixture.engine.isPlaying());
    }
    // RESTART while playing.
    {
        Fixture fixture;
        fixture.state.inputs = {InputFunction::Restart, InputFunction::Off};
        fixture.begin();
        const std::uint32_t restartsBefore = fixture.engine.resetRuntimeCountForTest();
        pulseOnInput1(fixture, 1000U);
        CHECK_EQ(locksInTick(fixture, 2100U), 0U);
        CHECK(fixture.engine.isPlaying());
        CHECK(fixture.engine.resetRuntimeCountForTest() > restartsBefore);
    }
    // RUN high, then low.
    {
        Fixture fixture;
        fixture.state.inputs = {InputFunction::Run, InputFunction::Off};
        fixture.begin();
        fixture.engine.stop();
        fixture.inputs.injectSyncEdgeForTest(1000U, true);
        CHECK_EQ(locksInTick(fixture, 1050U), 0U);
        CHECK(fixture.engine.isPlaying());
        fixture.inputs.injectSyncEdgeForTest(2000U, false);
        CHECK_EQ(locksInTick(fixture, 2050U), 0U);
        CHECK(!fixture.engine.isPlaying());
    }
}

void testSyncLockAndLossOpenNoLockInTheSchedulerTick() {
    Fixture fixture;
    fixture.state.source = ClockSource::External;
    fixture.state.externalSync.autostartOnLock = true;
    fixture.state.externalSync.lossMode = SyncLossMode::Stop;
    fixture.state.externalSync.timeoutMs = 100U;
    fixture.state.externalSync.glitchFilterUs = 1U;
    fixture.begin();
    fixture.engine.stop();
    fixture.sync.begin(fixture.state);

    // Second pulse acquires the lock and auto-starts.
    pulseOnInput1(fixture, 1000U);
    CHECK_EQ(locksInTick(fixture, 2050U), 0U);
    pulseOnInput1(fixture, 501000U);
    CHECK_EQ(locksInTick(fixture, 502050U), 0U);
    CHECK(fixture.engine.snapshot().externalLocked);
    CHECK(fixture.engine.isPlaying());

    // Timeout: lock lost, loss mode STOP stops the engine.
    CHECK_EQ(locksInTick(fixture, 3000000U), 0U);
    CHECK(!fixture.engine.snapshot().externalLocked);
    CHECK(!fixture.engine.isPlaying());
}

namespace commit_window_fake {
/** Controller and fixture the swap hook acts on. */
Fixture* fixture = nullptr;
bool stopDuringPreparation = false;

/** @brief Runs between preparation and swap: an input STOP from the scheduler. */
void hook(void*) {
    if (!stopDuringPreparation || fixture == nullptr) {
        return;
    }
    stopDuringPreparation = false;
    fixture->inputs.injectSyncEdgeForTest(9000U, false);
    fixture->tick(9050U);
}
}  // namespace commit_window_fake

void testClosingTheCommitWindowStartsOutsideItsLock() {
    // F-075: the deferred start is prepared outside the lock; closing the window
    // opens no nested lock and computes nothing under a lock.
    Fixture fixture;
    fixture.state.inputs = {InputFunction::Run, InputFunction::Off};
    fixture.begin();
    fixture.engine.stop();
    fixture.tick(1000U);
    CHECK(fixture.sync.tryOpenCommitWindow());
    fixture.inputs.injectSyncEdgeForTest(2000U, true);
    fixture.tick(2050U);
    CHECK(!fixture.engine.isPlaying());
    CHECK(fixture.sync.startDeferredForTest());

    const std::uint32_t nestedBefore = hal::InterruptLock::nestedCountForTest();
    const std::uint32_t underLockBefore = fixture.engine.scheduleCalculationsUnderLockForTest();
    fixture.sync.closeCommitWindow();
    CHECK_EQ(hal::InterruptLock::nestedCountForTest(), nestedBefore);
    CHECK_EQ(fixture.engine.scheduleCalculationsUnderLockForTest(), underLockBefore);
    CHECK(fixture.engine.isPlaying());
    CHECK(!fixture.sync.startDeferredForTest());
    TransportState transition{};
    CHECK(fixture.sync.consumeTransportTransition(transition));
    CHECK_EQ(transition, TransportState::Playing);
    // The window is closed: a later RUN edge acts at once.
    fixture.inputs.injectSyncEdgeForTest(3000U, false);
    fixture.tick(3050U);
    CHECK(!fixture.engine.isPlaying());
}

void testInputStopDuringTheDeferredStartPreparationWins() {
    Fixture fixture;
    fixture.state.inputs = {InputFunction::Run, InputFunction::Off};
    fixture.begin();
    fixture.engine.stop();
    fixture.tick(1000U);
    CHECK(fixture.sync.tryOpenCommitWindow());
    fixture.inputs.injectSyncEdgeForTest(2000U, true);
    fixture.tick(2050U);
    CHECK(fixture.sync.startDeferredForTest());

    commit_window_fake::fixture = &fixture;
    commit_window_fake::stopDuringPreparation = true;
    fixture.engine.setConfigurationSwapHookForTest(commit_window_fake::hook, nullptr);
    fixture.sync.closeCommitWindow();
    fixture.engine.setConfigurationSwapHookForTest(nullptr, nullptr);
    commit_window_fake::fixture = nullptr;

    CHECK(!fixture.engine.isPlaying());
    CHECK(!fixture.sync.startDeferredForTest());
    // Closed even though nothing started.
    CHECK(fixture.sync.tryOpenCommitWindow());
    fixture.sync.closeCommitWindow();
}
