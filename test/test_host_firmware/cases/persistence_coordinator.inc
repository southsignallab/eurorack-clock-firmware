/**
 * @file persistence_coordinator.inc
 * @brief P2 regressions: slot cache, table CRC, downgrade lock, central commit coordinator,
 * commit window, Custom Groove name cache, and complete factory reset.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

namespace {
std::uint32_t referenceBitwiseCrc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFUL;
    for (std::size_t index = 0U; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xEDB88320UL : crc >> 1U;
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/** Owns one complete set of foreground stores over the shared simulated Flash. */
struct PersistenceRig final {
    hal::PersistentStorage storage;
    services::PersistentStateService state{storage};
    services::CustomGrooveStore grooves{storage, true};
    services::SequencerPatternStore patterns{storage};
    services::SequencerStepStore steps{storage};
    services::PersistenceCoordinator coordinator{storage, state, grooves, patterns, steps};

    void begin() {
        state.begin();
        (void)patterns.begin();
        (void)steps.begin();
    }
};

/** Commit gate double: refuses or accepts and counts window usage. */
struct CountingGate final : hal::FlashCommitGate {
    bool refuse = false;
    std::uint32_t opened = 0U;
    std::uint32_t closed = 0U;
    bool tryOpenCommitWindow() override {
        if (refuse) {
            return false;
        }
        ++opened;
        return true;
    }
    void closeCommitWindow() override { ++closed; }
};

void commitCurrentBpm(std::uint16_t bpm, std::uint32_t nowMs) {
    hal::PersistentStorage storage;
    services::PersistentStateService service(storage);
    service.begin();
    ClockState state = makeDefaultState();
    state.bpm = bpm;
    service.requestCurrentState(state, nowMs);
    service.service(nowMs + config::kPersistenceCommitDelayMs);
}
}  // namespace

void testTableCrcMatchesHistoricalBitwiseCrc() {
    // F-009: the table CRC must be bit-identical, or every stored image becomes invalid.
    const char* const check = "123456789";
    CHECK_EQ(hal::crc32(reinterpret_cast<const std::uint8_t*>(check), 9U), 0xCBF43926UL);
    std::array<std::uint8_t, 4099U> data{};
    std::uint32_t seed = 0x1234567UL;
    for (std::uint8_t& byte : data) {
        seed = seed * 1103515245U + 12345U;
        byte = static_cast<std::uint8_t>(seed >> 16U);
    }
    for (const std::size_t size : {std::size_t{0U}, std::size_t{1U}, std::size_t{31U}, data.size()}) {
        CHECK_EQ(hal::crc32(data.data(), size), referenceBitwiseCrc32(data.data(), size));
    }
}

void testBootValidatesEachSlotOnlyOnce() {
    // F-009: both slots hold a committed generation and all presets are empty
    // (the historical worst case: nine legacy probes per empty preset).
    resetFakes();
    commitCurrentBpm(111U, 10U);
    commitCurrentBpm(112U, 20000U);
    hal::PersistentStorage::simulateRebootForTest();
    prepareDisplaySuccess();
    fakefw::delayAdvanceOverrideMs = 333U;
    app::ClockApplication application;
    application.begin();
    // Exactly one payload CRC per slot for the whole boot, however many records are read.
    CHECK_EQ(hal::PersistentStorage::payloadValidationCountForTest(), 2U);
    CHECK(hal::PersistentStorage::readCountForTest() >= 20U);
    application.runOnce();
    CHECK_EQ(hal::PersistentStorage::payloadValidationCountForTest(), 2U);
}

void testCommitVerifiesOnlyTheWrittenSlot() {
    resetFakes();
    commitCurrentBpm(111U, 10U);
    commitCurrentBpm(112U, 20000U);
    hal::PersistentStorage::simulateRebootForTest();
    commitCurrentBpm(113U, 40000U);
    // Boot scan (2) plus verification of the target slot (1).
    CHECK_EQ(hal::PersistentStorage::payloadValidationCountForTest(), 3U);
    hal::PersistentStorage::simulateRebootForTest();
    hal::PersistentStorage storage;
    services::PersistentStateService reloaded(storage);
    reloaded.begin();
    ClockState restored{};
    CHECK(reloaded.restoreCurrentState(restored));
    CHECK_EQ(restored.bpm, 113U);
}

void testForeignSlotFormatLocksWritesUntilFactoryReset() {
    // F-045: a newer firmware's slot in an unknown format must never be overwritten silently.
    resetFakes();
    commitCurrentBpm(140U, 10U);
    hal::PersistentStorage::seedForeignSlotForTest(1U, 2U, 9U);
    PersistenceRig rig;
    rig.begin();
    CHECK(rig.storage.writeLocked());
    ClockState restored{};
    CHECK(rig.state.restoreCurrentState(restored));
    CHECK_EQ(restored.bpm, 140U);  // older readable generation stays usable

    const std::uint32_t commitsBefore = hal::PersistentStorage::writeCommitCountForTest();
    CHECK(!rig.coordinator.service(100U, true));
    CHECK(rig.coordinator.status() == services::PersistenceCoordinator::Status::SaveLocked);
    const std::uint32_t noticesAtBoot = rig.coordinator.noticeRevision();
    CHECK(noticesAtBoot >= 1U);
    ClockState edited = makeDefaultState();
    edited.bpm = 141U;
    rig.state.requestCurrentState(edited, 200U);
    for (std::uint32_t now = 200U; now < 200U + 60000U; now += 50U) {
        CHECK(!rig.coordinator.service(now, true));
    }
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), commitsBefore);
    CHECK(rig.coordinator.noticeRevision() > noticesAtBoot);
    CHECK(rig.coordinator.noticeRevision() <= noticesAtBoot + 8U);  // backoff-limited
    CHECK(!rig.storage.writeBytes(0U, reinterpret_cast<const std::uint8_t*>("x"), 1U));
    CHECK(rig.storage.lastCommitOutcome() == hal::CommitOutcome::Locked);

    // ERASE ALL DATA is the explicit consent that replaces the foreign generation.
    ClockState factory = makeFactoryState();
    CHECK(rig.state.factoryReset(factory));
    CHECK(!rig.storage.writeLocked());
    (void)rig.coordinator.service(70000U, true);
    CHECK(rig.coordinator.status() == services::PersistenceCoordinator::Status::Ok);
    hal::PersistentStorage::simulateRebootForTest();
    hal::PersistentStorage verify;
    CHECK(!verify.writeLocked());
    CHECK(verify.writeBytes(0U, reinterpret_cast<const std::uint8_t*>("y"), 1U));
}

void testForeignSlotLockFollowsGenerationAndHeaderCrc() {
    resetFakes();
    hal::PersistentStorage::seedForeignSlotForTest(1U, 2U, 1U);
    hal::PersistentStorage storage;
    CHECK(storage.writeLocked());  // only committed slot is foreign: nothing may overwrite it
    commitCurrentBpm(150U, 10U);  // blocked as well
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 0U);

    // An older foreign generation below a newer readable one does not lock.
    resetFakes();
    commitCurrentBpm(150U, 10U);
    commitCurrentBpm(151U, 20000U);
    hal::PersistentStorage::seedForeignSlotForTest(0U, 2U, 1U);
    CHECK(!storage.writeLocked());
    commitCurrentBpm(152U, 40000U);
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 3U);

    // A foreign-looking header with a broken header CRC is torn data, not a newer format.
    resetFakes();
    commitCurrentBpm(153U, 10U);
    hal::PersistentStorage::seedForeignSlotForTest(1U, 2U, 9U, false);
    CHECK(!storage.writeLocked());
}

void testCoordinatorBundlesEveryStoreIntoOneCommit() {
    // D-04: one transaction (one sector erase) for CURRENT, a preset, a Custom
    // Groove, and both Sequencer banks.
    resetFakes();
    PersistenceRig rig;
    rig.begin();
    ClockState state = makeDefaultState();
    state.bpm = 133U;
    rig.state.requestCurrentState(state, 10U);
    CHECK(rig.state.savePreset(2U, "BUNDLE", state));
    CustomGroovePattern groove{};
    groove.length = 4U;
    groove.offsets256[1] = 12;
    CHECK(rig.grooves.save(3U, "GROOVE", groove));
    CHECK(rig.patterns.setGate(1U, 0U, 5U, true, 10U));
    SequencerStepMetadata metadata{};
    metadata.gateProfile = SequencerGateProfile::Gate20Ms;
    CHECK(rig.steps.updateMetadata(1U, 0U, 5U, metadata, 10U));

    CHECK(!rig.coordinator.service(11U, false));  // PLAY: nothing reaches Flash
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 0U);
    CHECK(rig.coordinator.service(12U, true));  // explicit saves are urgent
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 1U);
    CHECK(!rig.coordinator.hasPendingCommit());

    PersistenceRig reloaded;
    reloaded.begin();
    ClockState restored{};
    CHECK(reloaded.state.restoreCurrentState(restored));
    CHECK_EQ(restored.bpm, 133U);
    CHECK(reloaded.state.presetExists(2U));
    CHECK(reloaded.grooves.exists(3U));
    CHECK(reloaded.patterns.pattern(1U, 0U).gates[0] != 0ULL);
    CHECK(reloaded.steps.metadata(1U, 0U, 5U).gateProfile == SequencerGateProfile::Gate20Ms);
}

void testCoordinatorDebounceRestartsOnEveryChangeWithMaximumLatency() {
    // F-020: 60 s of editing with one change per second commits at most twice.
    resetFakes();
    PersistenceRig rig;
    rig.begin();
    std::uint32_t now = 1000U;
    std::uint32_t commits = 0U;
    for (std::uint32_t second = 0U; second < 60U; ++second) {
        // Toggle within the default 16-step length so every edit really changes the bank.
        const auto step = static_cast<std::uint8_t>(second % 16U);
        const bool current = (rig.patterns.pattern(0U, 0U).gates[step / 64U] >> (step % 64U)) & 1ULL;
        CHECK(rig.patterns.setGate(0U, 0U, step, !current, now));
        for (std::uint32_t pass = 0U; pass < 100U; ++pass, now += 10U) {
            commits += rig.coordinator.service(now, true) ? 1U : 0U;
        }
    }
    CHECK(commits <= 2U);
    CHECK(commits >= 1U);  // the 30-s ceiling forces progress during continuous editing
    // After the last change, the quiet period alone triggers the final commit.
    std::uint32_t settleCommits = 0U;
    for (std::uint32_t pass = 0U; pass < 400U; ++pass, now += 10U) {
        settleCommits += rig.coordinator.service(now, true) ? 1U : 0U;
    }
    CHECK_EQ(settleCommits, 1U);
    CHECK(!rig.patterns.dirty());

    // A single change waits for the quiet period, not longer.
    const bool firstGate = (rig.patterns.pattern(0U, 1U).gates[0] & 1ULL) != 0ULL;
    CHECK(rig.patterns.setGate(0U, 1U, 0U, !firstGate, now));
    const std::uint32_t changedAt = now;
    const std::uint32_t before = hal::PersistentStorage::writeCommitCountForTest();
    bool singleCommitted = false;
    while (!singleCommitted && now - changedAt <= config::kPersistenceCommitDelayMs + 10U) {
        singleCommitted = rig.coordinator.service(now, true);
        if (!singleCommitted) {
            now += 10U;
        }
    }
    CHECK(singleCommitted);
    CHECK(now - changedAt >= config::kPersistenceCommitDelayMs);
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), before + 1U);
}

void testCoordinatorBacksOffOnPersistentWriteFailure() {
    // F-021: a dauerhaft failing sector must not be erased in every foreground pass.
    CHECK_EQ(services::PersistenceCoordinator::retryDelayMs(1U), 1000U);
    CHECK_EQ(services::PersistenceCoordinator::retryDelayMs(2U), 2000U);
    CHECK_EQ(services::PersistenceCoordinator::retryDelayMs(6U), 32000U);
    CHECK_EQ(services::PersistenceCoordinator::retryDelayMs(7U), 60000U);
    CHECK_EQ(services::PersistenceCoordinator::retryDelayMs(200U), 60000U);

    resetFakes();
    PersistenceRig rig;
    rig.begin();
    ClockState state = makeDefaultState();
    CHECK(rig.state.savePreset(0U, "WORN", state));
    hal::PersistentStorage::failAllWritesForTest(true);
    std::uint32_t now = 100U;
    for (; now < 100U + 60000U; now += 5U) {
        CHECK(!rig.coordinator.service(now, true));
    }
    // Attempts at +0, +1, +3, +7, +15, +31 s: six in the first minute.
    CHECK_EQ(hal::PersistentStorage::commitAttemptCountForTest(), 6U);
    CHECK(rig.coordinator.status() == services::PersistenceCoordinator::Status::SaveError);
    CHECK_EQ(rig.coordinator.noticeRevision(), 6U);
    CHECK(rig.state.presetExists(0U));  // pending data is kept
    CHECK(rig.coordinator.hasPendingCommit());

    hal::PersistentStorage::failAllWritesForTest(false);
    bool committed = false;
    for (std::uint32_t pass = 0U; pass < 70000U / 5U && !committed; ++pass, now += 5U) {
        committed = rig.coordinator.service(now, true);
    }
    CHECK(committed);
    CHECK(rig.coordinator.status() == services::PersistenceCoordinator::Status::Ok);
    CHECK_EQ(rig.coordinator.consecutiveFailures(), 0U);
    PersistenceRig reloaded;
    reloaded.begin();
    CHECK(reloaded.state.presetExists(0U));
}

void testCommitGateRefusalIsDeferredNotFailure() {
    resetFakes();
    PersistenceRig rig;
    CountingGate gate;
    rig.storage.setCommitGate(&gate);
    rig.begin();
    ClockState state = makeDefaultState();
    CHECK(rig.state.savePreset(1U, "GATE", state));
    gate.refuse = true;
    CHECK(!rig.coordinator.service(10U, true));
    CHECK(rig.storage.lastCommitOutcome() == hal::CommitOutcome::Deferred);
    CHECK(rig.coordinator.status() == services::PersistenceCoordinator::Status::Ok);
    CHECK_EQ(rig.coordinator.consecutiveFailures(), 0U);
    CHECK_EQ(hal::PersistentStorage::commitAttemptCountForTest(), 0U);
    gate.refuse = false;
    CHECK(rig.coordinator.service(11U, true));  // no backoff after a deferral
    CHECK_EQ(gate.opened, 1U);
    CHECK_EQ(gate.closed, 1U);
}

void testCommitWindowDefersScheduledStartUntilClosed() {
    // D-04: a RUN edge during the Flash commit is executed after the commit.
    resetFakes();
    fakefw::setPinSilently(pinmap::kExternalSyncSignalPin, LOW);
    ClockState state = makeDefaultState();
    state.transport = TransportState::Stopped;
    state.inputs = {InputFunction::Run, InputFunction::Off};
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    engine::ClockEngine engine(gates);
    hal::ExternalInputCapture inputs;
    inputs.begin();
    services::ExternalSyncController sync(inputs, engine);
    engine.begin(state);
    sync.begin(state);
    sync.processSchedulerTick(10U);
    CHECK(!engine.isPlaying());

    CHECK(sync.tryOpenCommitWindow());
    fakefw::setPin(pinmap::kExternalSyncSignalPin, HIGH);
    sync.processSchedulerTick(100U);
    CHECK(!engine.isPlaying());
    CHECK(sync.startDeferredForTest());
    TransportState transition{};
    CHECK(!sync.consumeTransportTransition(transition));
    sync.closeCommitWindow();
    CHECK(engine.isPlaying());
    CHECK(sync.consumeTransportTransition(transition));
    CHECK_EQ(transition, TransportState::Playing);

    // A running engine refuses the window; Flash stays untouched.
    CHECK(!sync.tryOpenCommitWindow());

    // RUN LOW inside a window cancels a start recorded earlier in the same window.
    fakefw::setPin(pinmap::kExternalSyncSignalPin, LOW);
    sync.processSchedulerTick(200U);
    CHECK(!engine.isPlaying());
    (void)sync.consumeTransportTransition(transition);
    CHECK(sync.tryOpenCommitWindow());
    fakefw::setPin(pinmap::kExternalSyncSignalPin, HIGH);
    sync.processSchedulerTick(300U);
    fakefw::setPin(pinmap::kExternalSyncSignalPin, LOW);
    sync.processSchedulerTick(400U);
    CHECK(!sync.startDeferredForTest());
    sync.closeCommitWindow();
    CHECK(!engine.isPlaying());
}

void testCustomGrooveNamesAreServedFromRam() {
    // F-018: the Performance screen asks for a Custom Groove name every frame.
    resetFakes();
    hal::PersistentStorage storage;
    services::CustomGrooveStore direct(storage);
    CustomGroovePattern groove{};
    groove.length = 4U;
    CHECK(direct.save(4U, "SWAGGER  ", groove));
    services::CustomGrooveStore store(storage, true);
    char name[17]{};
    store.name(4U, name, sizeof(name));
    CHECK(std::strcmp(name, "SWAGGER") == 0);
    const std::uint32_t readsAfterFirst = hal::PersistentStorage::readCountForTest();
    for (int frame = 0; frame < 100; ++frame) {
        store.name(4U, name, sizeof(name));
        CHECK(store.exists(4U));
    }
    CHECK_EQ(hal::PersistentStorage::readCountForTest(), readsAfterFirst);
    char shortName[4]{};
    store.name(4U, shortName, sizeof(shortName));
    CHECK(std::strcmp(shortName, "SWA") == 0);

    CHECK(store.save(4U, "RENAMED", groove));
    store.name(4U, name, sizeof(name));
    CHECK(std::strcmp(name, "RENAMED") == 0);
    CHECK(store.clear(4U));
    store.name(4U, name, sizeof(name));
    CHECK(name[0] == '\0');
    CHECK(!store.exists(4U));
    CHECK(!store.exists(kCustomGrooveSlotCount));
    store.name(kCustomGrooveSlotCount, name, sizeof(name));
    CHECK(name[0] == '\0');
}

void testFactoryResetClearsEveryStoreAndEngineCopy() {
    // F-008: a dirty Sequencer bank and a queued Custom Groove must not survive ERASE ALL DATA.
    resetFakes();
    prepareDisplaySuccess();
    hal::OledDisplay display;
    CHECK(display.begin());
    ClockState state = makeDefaultState();
    state.transport = TransportState::Stopped;
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    engine::ClockEngine engine(gates);
    engine.begin(state);
    PersistenceRig rig;
    rig.begin();
    ui::UiRenderer renderer(display, rig.state, rig.grooves, rig.patterns, rig.steps);
    ui::UiController controller(state, engine, renderer, rig.state, rig.grooves, rig.patterns, rig.steps);
    std::uint32_t now = 1000U;

    CustomGroovePattern groove{};
    groove.length = 4U;
    groove.offsets256[2] = 20;
    CHECK(rig.grooves.save(1U, "OLDGROOVE", groove));
    engine.updateCustomGrooveSlot(1U, groove, false);
    SequencerPatternV2 pattern = rig.patterns.pattern(0U, 0U);
    pattern.gates[0] = 0xF0F0ULL;
    CHECK(rig.patterns.updatePattern(0U, 0U, pattern, now));
    engine.updateSequencerPattern(0U, pattern, false);
    SequencerStepMetadata metadata{};
    metadata.gateProfile = SequencerGateProfile::Gate20Ms;
    CHECK(rig.steps.updateMetadata(0U, 0U, 3U, metadata, now));
    CHECK(rig.grooves.hasPendingCommit());
    CHECK(rig.patterns.dirty());

    controllerOpenSettingsChord(controller, now);
    controllerTurn(controller, 4, now);  // INFO
    controllerShortPress(controller, now);
    controllerTurn(controller, 5, now);  // FACTORY RESET
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::FactoryResetConfirm);
    controllerTurn(controller, 1, now);  // YES
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::Performance);

    CHECK(!rig.grooves.hasPendingCommit());
    CHECK(!rig.steps.dirty());
    CHECK(!rig.grooves.exists(1U));
    // P1 is re-seeded from the factory legacy sequence, exactly like a boot
    // without a durable bank; the edited pattern is gone from store and engine.
    const std::uint64_t factoryGates = makeFactoryState().channels[0].sequencer.pattern;
    CHECK_EQ(rig.patterns.pattern(0U, 0U).gates[0], factoryGates);
    CHECK_EQ(engine.sequencerPatternForTest(0U).gates[0], factoryGates);
    CHECK(rig.patterns.dirty());
    CHECK_EQ(engine.customGrooveSlotForTest(1U).offsets256[2], 0);

    // Ten seconds of foreground passes write the factory bank once and nothing old.
    const std::uint32_t commitsAfterReset = hal::PersistentStorage::writeCommitCountForTest();
    for (std::uint32_t pass = 0U; pass < 1000U; ++pass, now += 10U) {
        (void)rig.coordinator.service(now, true);
    }
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), commitsAfterReset + 1U);
    PersistenceRig reloaded;
    reloaded.begin();
    CHECK(!reloaded.grooves.exists(1U));
    CHECK(reloaded.patterns.hasDurableBank());
    CHECK_EQ(reloaded.patterns.pattern(0U, 0U).gates[0], factoryGates);
    CHECK(reloaded.steps.metadata(0U, 0U, 3U).gateProfile != SequencerGateProfile::Gate20Ms);
}

void testPersistenceNoticeIsShownAndExpires() {
    resetFakes();
    prepareDisplaySuccess();
    hal::OledDisplay display;
    CHECK(display.begin());
    ClockState state = makeDefaultState();
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    engine::ClockEngine engine(gates);
    engine.begin(state);
    hal::PersistentStorage storage;
    services::PersistentStateService persistence(storage);
    persistence.begin();
    ui::UiRenderer renderer(display, persistence);
    ui::UiController controller(state, engine, renderer, persistence);
    controller.showPersistenceNotice(ui::PersistenceNotice::SaveError, 100U);
    CHECK(controller.navigation().persistenceNotice == ui::PersistenceNotice::SaveError);
    controller.serviceRendering(100U + config::kDisplayRefreshMinimumMs);
    controller.showPersistenceNotice(ui::PersistenceNotice::SaveLocked, 200U);
    controller.serviceRendering(200U + 2U * config::kDisplayRefreshMinimumMs);
    CHECK(controller.navigation().persistenceNotice == ui::PersistenceNotice::SaveLocked);
    controller.serviceRendering(200U + config::kPersistenceNoticeMs);
    CHECK(controller.navigation().persistenceNotice == ui::PersistenceNotice::None);
    CHECK(std::strcmp(text::get(text::TextId::SaveErrorNotice), "SAVE ERROR") == 0);
    CHECK(std::strcmp(text::get(text::TextId::SaveLockedNotice), "SAVE LOCKED") == 0);
}

void testApplicationAnnouncesSaveErrorFromCoordinator() {
    resetFakes();
    prepareDisplaySuccess();
    fakefw::delayAdvanceOverrideMs = 333U;
    ClockState initial = makeDefaultState();
    initial.transport = TransportState::Stopped;
    app::ClockApplication application;
    application.begin(initial);
    hal::PersistentStorage::failAllWritesForTest(true);
    // PLAY then STOP changes the durable transport preference; after the quiet
    // period the commit fails.
    fakefw::setPin(pinmap::kPlayPauseButtonPin, LOW);
    runPasses(application, fakefw::nowMs + 60U);
    fakefw::setPin(pinmap::kPlayPauseButtonPin, HIGH);
    runPasses(application, fakefw::nowMs + 60U);
    pressStop(application);
    CHECK(application.persistenceForTest().hasPendingCommit());
    runPasses(application, fakefw::nowMs + config::kPersistenceCommitDelayMs + 200U);
    CHECK(application.persistenceForTest().status() == services::PersistenceCoordinator::Status::SaveError);
    CHECK(application.navigationForTest().persistenceNotice == ui::PersistenceNotice::SaveError);
    hal::PersistentStorage::failAllWritesForTest(false);
}

void testPersistenceEdgePaths() {
    // A foreign slot appearing between begin and commit still blocks the commit.
    resetFakes();
    {
        hal::PersistentStorage storage;
        CHECK(storage.beginUpdate());
        const std::uint8_t byte = 0x5AU;
        CHECK(storage.stageBytes(0U, &byte, 1U));
        hal::PersistentStorage::seedForeignSlotForTest(1U, 2U, 3U);
        CHECK(!storage.commitUpdate());
        CHECK(storage.lastCommitOutcome() == hal::CommitOutcome::Locked);
    }
    // Known format version, unknown payload size: also foreign.
    resetFakes();
    hal::PersistentStorage::seedForeignSlotForTest(0U, 1U, 4U, true, 6000U);
    {
        hal::PersistentStorage storage;
        CHECK(storage.writeLocked());
    }

    // Standalone Custom Groove service paths.
    resetFakes();
    {
        hal::PersistentStorage storage;
        services::CustomGrooveStore immediate(storage);
        CHECK(immediate.service(true));  // not deferred: nothing to do
        services::CustomGrooveStore deferred(storage, true);
        CustomGroovePattern groove{};
        groove.length = 4U;
        CHECK(deferred.save(0U, "EDGE", groove));
        hal::PersistentStorage::failNextReadForTest();
        CHECK(!deferred.service(true));
        CHECK(deferred.hasPendingCommit());
        CHECK(deferred.service(true));
        CHECK(!deferred.hasPendingCommit());
    }

    // Standalone CURRENT/preset service keeps everything on a failed commit.
    resetFakes();
    {
        hal::PersistentStorage storage;
        services::PersistentStateService service(storage);
        service.begin();
        CHECK(service.savePreset(5U, "KEEP", makeDefaultState()));
        hal::PersistentStorage::failNextWriteForTest();
        service.service(10U);
        CHECK(service.hasUrgentCommit());
        service.service(11U);
        CHECK(!service.hasPendingCommit());
    }

    // Failure counter saturates instead of wrapping.
    resetFakes();
    {
        PersistenceRig rig;
        rig.begin();
        CHECK(rig.state.savePreset(0U, "SAT", makeDefaultState()));
        hal::PersistentStorage::failAllWritesForTest(true);
        std::uint32_t now = 0U;
        for (int attempt = 0; attempt < 260; ++attempt) {
            (void)rig.coordinator.service(now, true);
            now += config::kPersistenceRetryMaximumMs + 1U;
        }
        CHECK_EQ(rig.coordinator.consecutiveFailures(), UINT8_MAX);
        hal::PersistentStorage::failAllWritesForTest(false);
    }
}

void testFactoryResetFailureAndStorelessController() {
    resetFakes();
    prepareDisplaySuccess();
    hal::OledDisplay display;
    CHECK(display.begin());
    ClockState state = makeDefaultState();
    state.transport = TransportState::Stopped;
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    engine::ClockEngine engine(gates);
    engine.begin(state);
    std::uint32_t now = 1000U;
    auto confirmReset = [&now](ui::UiController& controller) {
        controllerOpenSettingsChord(controller, now);
        controllerTurn(controller, 4, now);
        controllerShortPress(controller, now);
        controllerTurn(controller, 5, now);
        controllerShortPress(controller, now);
        controllerTurn(controller, 1, now);
        controllerShortPress(controller, now);
    };
    {
        // A failed image replacement leaves RAM and Flash on the previous data.
        PersistenceRig rig;
        rig.begin();
        ui::UiRenderer renderer(display, rig.state, rig.grooves, rig.patterns, rig.steps);
        ui::UiController controller(state, engine, renderer, rig.state, rig.grooves, rig.patterns, rig.steps);
        CustomGroovePattern groove{};
        groove.length = 4U;
        CHECK(rig.grooves.save(2U, "STAYS", groove));
        hal::PersistentStorage::failAllWritesForTest(true);
        confirmReset(controller);
        hal::PersistentStorage::failAllWritesForTest(false);
        CHECK_EQ(controller.navigation().screen, ui::Screen::Settings);
        CHECK(rig.grooves.hasPendingCommit());
        CHECK(rig.grooves.exists(2U));
    }
    {
        // Controllers without Groove/Sequencer stores reset CURRENT only.
        hal::PersistentStorage storage;
        services::PersistentStateService persistence(storage);
        persistence.begin();
        ui::UiRenderer renderer(display, persistence);
        ui::UiController controller(state, engine, renderer, persistence);
        confirmReset(controller);
        CHECK_EQ(controller.navigation().screen, ui::Screen::Performance);
    }
}

void testApplicationAnnouncesSaveLockedAtBoot() {
    resetFakes();
    commitCurrentBpm(125U, 10U);
    hal::PersistentStorage::seedForeignSlotForTest(1U, 2U, 9U);
    prepareDisplaySuccess();
    fakefw::delayAdvanceOverrideMs = 333U;
    app::ClockApplication application;
    application.begin();
    application.runOnce();
    CHECK(application.persistenceForTest().status() == services::PersistenceCoordinator::Status::SaveLocked);
    CHECK(application.navigationForTest().persistenceNotice == ui::PersistenceNotice::SaveLocked);
}
