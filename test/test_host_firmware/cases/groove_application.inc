/**
 * @file groove_application.inc
 * @brief Included test cases split from test_host_firmware/test_main.cpp for readability.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

void testCustomGrooveEditorControllerWorkflow() {
    resetFakes();
    prepareDisplaySuccess();
    hal::OledDisplay display;
    CHECK(display.begin());

    ClockState state = makeDefaultState();
    state.operatingMode = OperatingMode::UnifiedClock;
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    engine::ClockEngine engine(gates);
    engine.begin(state);
    hal::PersistentStorage::resetForTest();
    hal::PersistentStorage storage;
    services::PersistentStateService persistentState(storage);
    persistentState.begin();
    services::CustomGrooveStore grooveStore(storage);
    ui::UiRenderer renderer(display, persistentState, grooveStore);
    ui::UiController controller(state, engine, renderer, persistentState, grooveStore);
    std::uint32_t now = 100U;

    auto openUnifiedEditor = [&]() {
        controllerOpenSettingsChord(controller, now);
        controllerTurn(controller, 1, now); // CHANNEL SETTINGS
        controllerShortPress(controller, now);
        CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::UnifiedClock);
        controllerTurn(controller, 1, now); // TIMING
        controllerShortPress(controller, now);
        controllerTurn(controller, 4, now); // GROOVE
        controllerShortPress(controller, now);
        CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::Groove);
        controllerTurn(controller, 4, now); // EDITOR
        controllerShortPress(controller, now);
        CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveEditor);
    };

    openUnifiedEditor();
    CHECK_EQ(controller.navigation().grooveDraft.length, 16U);
    CHECK_EQ(controller.navigation().grooveCursor, 0U);

    // Fine and coarse marker movement cover both signed clamp ends.
    controllerTurn(controller, 127, now);
    CHECK_EQ(controller.navigation().grooveDraft.offsets256[0], kCustomGrooveMaximumOffset256);
    hal::ControlSample sample{};
    sample.tapButton = heldButton();
    sample.encoderDelta = -127;
    controller.processControls(sample, now++);
    CHECK_EQ(controller.navigation().grooveDraft.offsets256[0], kCustomGrooveMinimumOffset256);

    // TAP advances the marker. START+TURN is consumed as zoom and must not toggle transport.
    controllerTapAt(controller, now + 2U);
    now += 3U;
    CHECK_EQ(controller.navigation().grooveCursor, 1U);
    sample = {};
    sample.transportButton = heldButton();
    sample.encoderDelta = 1;
    controller.processControls(sample, now++);
    CHECK(controller.navigation().grooveZoomSteps != 0U);
    sample = {};
    sample.transportButton = releasedEdge();
    controller.processControls(sample, now++);
    CHECK_EQ(state.transport, TransportState::Stopped);

    // Long press opens the normal Groove menu. Exercise ZOOM and LENGTH edits.
    controllerLongPress(controller, now);
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::GrooveEditorMenu);
    controllerTurn(controller, 2, now); // ZOOM
    controllerShortPress(controller, now);
    CHECK(controller.navigation().editing);
    controllerTurn(controller, 1, now);
    controllerTurn(controller, -1, now);
    controllerShortPress(controller, now);
    controllerTurn(controller, 1, now); // LENGTH
    controllerShortPress(controller, now);
    controllerTurn(controller, -127, now);
    CHECK_EQ(controller.navigation().grooveDraft.length, 1U);
    CHECK_EQ(controller.navigation().grooveCursor, 0U);
    controllerTurn(controller, 127, now);
    CHECK_EQ(controller.navigation().grooveDraft.length, kCustomGrooveMaximumSteps);
    controllerShortPress(controller, now);
    controllerReset(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveEditor);

    // SAVE into a new slot. A generated musical name is present immediately and
    // encoder long-press commits it without forcing the user through 16 characters.
    controller.seedGeneratedNames(0x12345678U);
    controllerLongPress(controller, now);
    controllerShortPress(controller, now); // SAVE
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    controllerShortPress(controller, now); // empty slot 0 -> name entry
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveNameEntry);
    CHECK(controller.navigation().grooveNameBuffer[0] != ' ');
    CHECK(controller.navigation().grooveNameBuffer[0] != '\0');
    controllerTurn(controller, 1, now); // generated default remains editable
    controllerLongPress(controller, now); // explicit HOLD TO SAVE contract
    CHECK(grooveStore.exists(0U));
    char savedGrooveName[services::CustomGrooveStore::kNameLength + 1U]{};
    grooveStore.name(0U, savedGrooveName, sizeof(savedGrooveName));
    CHECK(savedGrooveName[0] != '\0');
    CHECK_EQ(state.unifiedClock.groove.preset, GroovePreset::Custom);
    CHECK_EQ(state.unifiedClock.groove.customSlot, 0U);
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::Groove);
    CHECK_EQ(controller.navigation().cursor, 4U);

    // Normal Groove settings expose LOAD directly; loading activates the stored
    // Custom Groove without first entering the graphical editor.
    state.unifiedClock.groove = {GroovePreset::Off, 100U, 0U, 0U};
    engine.updateConfiguration(state, true);
    controllerTurn(controller, -1, now); // LOAD
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    controllerShortPress(controller, now); // slot 0
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::Groove);
    CHECK_EQ(state.unifiedClock.groove.preset, GroovePreset::Custom);
    CHECK_EQ(state.unifiedClock.groove.customSlot, 0U);

    // Reopening an active Custom Groove must load the stored slot into the draft.
    controllerTurn(controller, 1, now); // EDITOR
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveEditor);
    CHECK_EQ(controller.navigation().grooveDraft.length, kCustomGrooveMaximumSteps);

    // Dirty LOAD first cancels, then confirms discard and reaches the slot list.
    controllerTurn(controller, 1, now);
    controllerLongPress(controller, now);
    controllerTurn(controller, 1, now); // LOAD
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveDiscardConfirm);
    controllerShortPress(controller, now); // NO
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveEditor);
    controllerLongPress(controller, now);
    controllerTurn(controller, 1, now);
    controllerShortPress(controller, now);
    controllerTurn(controller, 1, now); // YES
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);

    // Failed load remains in the list; successful load returns to the editor and clears dirty state.
    controllerTurn(controller, 9, now);
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    controllerTurn(controller, -9, now);
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveEditor);
    controllerReset(controller, now); // clean editor returns immediately
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::Groove);

    // Dirty BACK confirmation covers both NO and YES paths.
    controllerShortPress(controller, now);
    controllerTurn(controller, 1, now);
    controllerReset(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveDiscardConfirm);
    controllerReset(controller, now); // cancel with BACK
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveEditor);
    controllerReset(controller, now);
    controllerTurn(controller, 1, now);
    controllerShortPress(controller, now); // YES -> restore original settings
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::Groove);
    CHECK_EQ(state.unifiedClock.groove.preset, GroovePreset::Custom);

    // Overwrite flow: NO keeps slot selection; YES reuses the durable name and commits.
    controllerShortPress(controller, now);
    controllerLongPress(controller, now);
    controllerShortPress(controller, now); // SAVE
    controllerShortPress(controller, now); // existing slot -> overwrite confirm
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveOverwriteConfirm);
    controllerShortPress(controller, now); // NO
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    controllerReset(controller, now); // slots -> editor
    controllerLongPress(controller, now);
    controllerShortPress(controller, now);
    controllerShortPress(controller, now);
    controllerTurn(controller, 1, now);
    controllerShortPress(controller, now); // YES
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::Groove);

    // Rename an occupied Custom Groove from the normal Groove page. The name
    // editor reuses the existing name rather than generating a new one.
    char nameBeforeRename[services::CustomGrooveStore::kNameLength + 1U]{};
    grooveStore.name(0U, nameBeforeRename, sizeof(nameBeforeRename));
    controllerTurn(controller, 2, now); // RENAME
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    controllerShortPress(controller, now); // slot 0
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveNameEntry);
    CHECK_EQ(controller.navigation().grooveSlotAction, ui::GrooveSlotAction::Rename);
    CHECK(controller.navigation().grooveNameBuffer[0] == nameBeforeRename[0]);
    controllerTurn(controller, 1, now);
    hal::PersistentStorage::powerLossBeforeCommitForTest();
    controllerLongPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveNameEntry);
    char nameAfterFailedRename[services::CustomGrooveStore::kNameLength + 1U]{};
    grooveStore.name(0U, nameAfterFailedRename, sizeof(nameAfterFailedRename));
    CHECK(std::strcmp(nameBeforeRename, nameAfterFailedRename) == 0);
    controllerLongPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    char nameAfterRename[services::CustomGrooveStore::kNameLength + 1U]{};
    grooveStore.name(0U, nameAfterRename, sizeof(nameAfterRename));
    CHECK(std::strcmp(nameBeforeRename, nameAfterRename) != 0);

    // Delete requires an explicit YES. NO must be a true no-op.
    CustomGroovePattern disposable{};
    disposable.length = 4U;
    disposable.offsets256[1] = 32;
    CHECK(grooveStore.save(1U, "DELETE ME", disposable));
    controllerReset(controller, now); // RENAME slot list -> Groove
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::Groove);
    CHECK_EQ(controller.navigation().cursor, 6U);
    controllerTurn(controller, 1, now); // DELETE
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    controllerTurn(controller, 1, now); // slot 1
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveDeleteConfirm);
    controllerShortPress(controller, now); // NO
    CHECK(grooveStore.exists(1U));
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    controllerShortPress(controller, now); // slot 1 again
    controllerTurn(controller, 1, now); // YES
    controllerShortPress(controller, now);
    CHECK(!grooveStore.exists(1U));
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    controllerReset(controller, now); // DELETE slot list -> Groove

    // Independent mode exercises the per-channel preview/update/restore branches.
    state.operatingMode = OperatingMode::Independent;
    state.channels[0].common.mode = ChannelMode::Clock;
    state.channels[0].common.groove = {GroovePreset::Custom, 75U, 0U, 0U};
    engine.updateConfiguration(state, true);
    controllerReset(controller, now); // Groove -> ChannelTiming
    controllerReset(controller, now); // -> Channel
    controllerReset(controller, now); // -> Root
    controllerReset(controller, now); // -> Performance
    controllerOpenSettingsChord(controller, now);
    controllerTurn(controller, 1, now);
    controllerShortPress(controller, now); // Channel
    controllerTurn(controller, 1, now); // Timing
    controllerShortPress(controller, now);
    controllerTurn(controller, 4, now); // Groove
    controllerShortPress(controller, now);
    controllerTurn(controller, 4, now); // Editor
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveEditor);
    controllerTurn(controller, -1, now);
    controllerReset(controller, now);
    controllerTurn(controller, 1, now);
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::Groove);
    CHECK_EQ(state.channels[0].common.groove.preset, GroovePreset::Custom);
}

void testCustomGrooveUiDefersFlashUntilTransportStops() {
    resetFakes();
    prepareDisplaySuccess();
    hal::OledDisplay display;
    CHECK(display.begin());
    ClockState state = makeDefaultState();
    state.operatingMode = OperatingMode::UnifiedClock;
    state.transport = TransportState::Playing;
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    engine::ClockEngine engine(gates);
    engine.begin(state);
    hal::PersistentStorage::resetForTest();
    hal::PersistentStorage storage;
    services::PersistentStateService persistentState(storage);
    persistentState.begin();
    services::CustomGrooveStore grooveStore(storage, true);
    ui::UiRenderer renderer(display, persistentState, grooveStore);
    ui::UiController controller(state, engine, renderer, persistentState, grooveStore);
    std::uint32_t now = 100U;

    // Exercise production UI actions with the production store's deferred policy.
    controllerOpenSettingsChord(controller, now);
    controllerTurn(controller, 1, now); // CHANNEL SETTINGS
    controllerShortPress(controller, now);
    controllerTurn(controller, 1, now); // TIMING
    controllerShortPress(controller, now);
    controllerTurn(controller, 4, now); // GROOVE
    controllerShortPress(controller, now);
    controllerTurn(controller, 4, now); // EDITOR
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveEditor);

    controllerLongPress(controller, now);
    controllerShortPress(controller, now); // SAVE
    controllerShortPress(controller, now); // empty slot 0
    controllerLongPress(controller, now); // confirm generated name
    CHECK(grooveStore.exists(0U));
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 0U);
    CHECK(grooveStore.service(false));
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 0U);
    state.transport = TransportState::Stopped;
    CHECK(grooveStore.service(true));
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 1U);

    state.transport = TransportState::Playing;
    controllerTurn(controller, 2, now); // RENAME
    controllerShortPress(controller, now);
    controllerShortPress(controller, now); // slot 0
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveNameEntry);
    controllerTurn(controller, 1, now);
    controllerLongPress(controller, now); // rename
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 1U);
    CHECK(grooveStore.service(false));
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 1U);
    state.transport = TransportState::Stopped;
    CHECK(grooveStore.service(true));
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 2U);

    state.transport = TransportState::Playing;
    controllerReset(controller, now); // RENAME slots -> GROOVE
    controllerTurn(controller, 1, now); // DELETE
    controllerShortPress(controller, now);
    controllerShortPress(controller, now); // slot 0
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveDeleteConfirm);
    controllerTurn(controller, 1, now); // YES
    controllerShortPress(controller, now);
    CHECK(!grooveStore.exists(0U));
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 2U);
    CHECK(grooveStore.service(false));
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 2U);
    state.transport = TransportState::Stopped;
    CHECK(grooveStore.service(true));
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), 3U);
    services::CustomGrooveStore recovered(storage);
    CHECK(!recovered.exists(0U));
}

void testCustomGrooveRecorderControllerWorkflow() {
    resetFakes();
    prepareDisplaySuccess();
    hal::OledDisplay display;
    CHECK(display.begin());

    ClockState state = makeDefaultState();
    state.operatingMode = OperatingMode::UnifiedClock;
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    engine::ClockEngine engine(gates);
    engine.begin(state);
    hal::PersistentStorage::resetForTest();
    hal::PersistentStorage storage;
    services::PersistentStateService persistentState(storage);
    persistentState.begin();
    services::CustomGrooveStore grooveStore(storage);
    ui::UiRenderer renderer(display, persistentState, grooveStore);
    ui::UiController controller(state, engine, renderer, persistentState, grooveStore);
    std::uint32_t now = 100U;

    // SETTINGS -> CHANNEL SETTINGS -> TIMING -> GROOVE -> RECORD.
    controllerOpenSettingsChord(controller, now);
    controllerTurn(controller, 1, now);
    controllerShortPress(controller, now);
    controllerTurn(controller, 1, now);
    controllerShortPress(controller, now);
    controllerTurn(controller, 4, now);
    controllerShortPress(controller, now);
    controllerTurn(controller, 5, now);
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveRecorder);
    CHECK_EQ(controller.navigation().grooveRecordCountInBeats, 4U);
    CHECK_EQ(controller.navigation().grooveRecordMode, ui::GrooveRecordMode::OneShot);
    CHECK_EQ(controller.navigation().grooveRecordState, ui::GrooveRecordState::Ready);

    // Default Count-In is a recorder-local four-beat pre-roll. PLAY starts the
    // global clock if needed, then enters PRECOUNT; PLAY again cancels only the
    // recorder and rewinds its playhead.
    hal::ControlSample sample{};
    sample.transportButton = releasedEdge();
    controller.processControls(sample, now++);
    CHECK_EQ(state.transport, TransportState::Playing);
    CHECK_EQ(controller.navigation().grooveRecordState, ui::GrooveRecordState::PreCount);
    CHECK_EQ(controller.navigation().grooveRecordCountInRemaining, 4U);
    sample = {};
    sample.transportButton = releasedEdge();
    controller.processControls(sample, now++);
    CHECK_EQ(controller.navigation().grooveRecordState, ui::GrooveRecordState::Ready);
    CHECK_EQ(controller.navigation().grooveRecordPlayheadStep, 0U);

    // Record menu: switch to ENDLESS, turn Count-In off and use a four-step pattern.
    controllerLongPress(controller, now);
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::GrooveRecordMenu);
    controllerShortPress(controller, now); // MODE edit
    controllerTurn(controller, 1, now);
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().grooveRecordMode, ui::GrooveRecordMode::Endless);
    controllerTurn(controller, 1, now); // COUNT IN
    controllerShortPress(controller, now);
    controllerTurn(controller, -4, now);
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().grooveRecordCountInBeats, 0U);
    controllerTurn(controller, 1, now); // LENGTH
    controllerShortPress(controller, now);
    controllerTurn(controller, -12, now);
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().grooveDraft.length, 4U);
    controllerReset(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveRecorder);

    // PLAY starts recording and, from STOP, starts only the global musical clock.
    sample = {};
    sample.transportButton = releasedEdge();
    const std::uint32_t playNowUs = now * 1000U;
    controller.processControls(sample, now++, playNowUs);
    CHECK_EQ(state.transport, TransportState::Playing);
    CHECK_EQ(controller.navigation().grooveRecordState, ui::GrooveRecordState::Recording);

    // TAP is a record event here, not Tap Tempo. Capture straight step 1 at the
    // current engine position using the high-resolution physical edge timestamp.
    const std::uint16_t bpmBeforeTap = state.bpm;
    sample = {};
    sample.tapButton = {hal::ButtonEdge::Pressed, true, 1'000'000U};
    controller.processControls(sample, now++, 1'000'000U);
    CHECK_EQ(state.bpm, bpmBeforeTap);
    CHECK((controller.navigation().grooveRecordCapturedMask & 0x1ULL) != 0ULL);
    CHECK_EQ(controller.navigation().grooveDraft.offsets256[0], 0);

    // Advance slightly beyond the next nominal grid point and capture another
    // marker. The recorded offset must become visible in the shared draft.
    const std::uint64_t intervalQ32 = engine.nominalIntervalQ32(0U);
    const std::uint64_t targetQ32 = intervalQ32 + intervalQ32 / 8ULL;
    std::uint32_t guard = 0U;
    while (engine.snapshot().masterPositionQ32 < targetQ32 && guard++ < 20'000U) {
        engine.processSchedulerTick();
    }
    CHECK(guard < 20'000U);
    sample = {};
    sample.tapButton = {hal::ButtonEdge::Pressed, true, 2'000'000U};
    controller.processControls(sample, now++, 2'000'000U);
    CHECK((controller.navigation().grooveRecordCapturedMask & 0x2ULL) != 0ULL);
    CHECK(controller.navigation().grooveDraft.offsets256[1] > 0);

    // BACK stops capture before presenting the normal dirty-draft confirmation.
    // Cancelling that dialog returns to RECORD in READY; recording never runs
    // invisibly behind the modal screen.
    controllerReset(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveDiscardConfirm);
    CHECK_EQ(controller.navigation().grooveRecordState, ui::GrooveRecordState::Ready);
    controllerReset(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveRecorder);
    CHECK_EQ(controller.navigation().grooveRecordState, ui::GrooveRecordState::Ready);

    sample = {};
    sample.transportButton = releasedEdge();
    controller.processControls(sample, now++);
    CHECK_EQ(controller.navigation().grooveRecordState, ui::GrooveRecordState::Recording);

    // PLAY+TURN is consumed by zoom and must not stop the active recorder.
    const std::uint8_t zoomBefore = controller.navigation().grooveZoomSteps;
    sample = {};
    sample.transportButton = heldButton();
    sample.encoderDelta = 1;
    controller.processControls(sample, now++);
    sample = {};
    sample.transportButton = releasedEdge();
    controller.processControls(sample, now++);
    CHECK(controller.navigation().grooveZoomSteps != zoomBefore);
    CHECK_EQ(controller.navigation().grooveRecordState, ui::GrooveRecordState::Recording);

    // PLAY alone stops recording and rewinds the recorder playhead without
    // stopping the global transport that supplies the clock.
    sample = {};
    sample.transportButton = releasedEdge();
    controller.processControls(sample, now++);
    CHECK_EQ(controller.navigation().grooveRecordState, ui::GrooveRecordState::Ready);
    CHECK_EQ(controller.navigation().grooveRecordPlayheadStep, 0U);
    CHECK_EQ(controller.navigation().grooveRecordPlayheadPhase256, 0U);
    CHECK_EQ(state.transport, TransportState::Playing);

    // SAVE from the Record menu uses the same durable Custom Groove store as
    // the manual editor. This is the direct recorder save path requested by the UI.
    const auto capturedDraft = controller.navigation().grooveDraft;
    controllerLongPress(controller, now);
    controllerTurn(controller, 4, now); // SAVE
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveSlots);
    controllerShortPress(controller, now); // slot 0 -> generated name
    controllerLongPress(controller, now);
    CHECK(grooveStore.exists(0U));
    CustomGroovePattern stored{};
    CHECK(grooveStore.load(0U, stored));
    CHECK_EQ(stored.length, capturedDraft.length);
    CHECK_EQ(stored.offsets256[0], capturedDraft.offsets256[0]);
    CHECK_EQ(stored.offsets256[1], capturedDraft.offsets256[1]);
    CHECK_EQ(controller.navigation().settingsPage, ui::SettingsPage::Groove);
    CHECK_EQ(controller.navigation().cursor, 5U); // RECORD

    // Reopening RECORD loads the stored pattern. EDITOR then receives the exact
    // same draft for normal marker-by-marker correction.
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveRecorder);
    CHECK_EQ(controller.navigation().grooveDraft.offsets256[1], capturedDraft.offsets256[1]);
    controllerLongPress(controller, now);
    controllerTurn(controller, 5, now); // EDITOR
    controllerShortPress(controller, now);
    CHECK_EQ(controller.navigation().screen, ui::Screen::GrooveEditor);
    CHECK_EQ(controller.navigation().grooveDraft.length, capturedDraft.length);
    CHECK_EQ(controller.navigation().grooveDraft.offsets256[0], capturedDraft.offsets256[0]);
    CHECK_EQ(controller.navigation().grooveDraft.offsets256[1], capturedDraft.offsets256[1]);
}


void testApplicationAndEntryPoints() {
#if !CLOCK_DISPLAY_USE_SPI
    resetFakes(); // failure path, abort infinite safe halt via fake delay exception
    fakefw::throwOnDelay=true;
    try { app::ClockApplication failed; failed.begin(); CHECK(false); } catch(const std::runtime_error&) { CHECK(true); }
#endif
    resetFakes(); prepareDisplaySuccess(); fakefw::delayAdvanceOverrideMs = 333U;
    app::ClockApplication application; application.begin(); CHECK(fakefw::nowMs>=config::kBootDurationMs); application.runOnce();
    resetFakes(); prepareDisplaySuccess(); fakefw::delayAdvanceOverrideMs = 333U;
    ClockState stoppedState=makeDefaultState(); stoppedState.transport=TransportState::Stopped;
    app::ClockApplication stoppedApplication; stoppedApplication.begin(stoppedState); stoppedApplication.runOnce();
    CHECK(HardwareTimer::lastInstance!=nullptr); HardwareTimer::lastInstance->fire();

    // Regression: a persisted PLAY state is remembered as metadata, but power-up
    // must still enter STOP and must never emit a gate until the user explicitly plays.
    resetFakes();
    {
        hal::PersistentStorage seedStorage;
        services::PersistentStateService seedState(seedStorage);
        seedState.begin();
        seedState.requestTransportState(TransportState::Playing, 10U);
        seedState.service(10U + config::kPersistenceCommitDelayMs);
        CHECK(seedState.hasStoredTransportState());
        CHECK_EQ(seedState.storedTransportState(), TransportState::Playing);
    }
    prepareDisplaySuccess();
    fakefw::delayAdvanceOverrideMs = 333U;
    app::ClockApplication persistedPlayApplication;
    persistedPlayApplication.begin();
    CHECK(HardwareTimer::lastInstance != nullptr);

    // Confirm that begin() did not erase or silently rewrite the historical state.
    {
        hal::PersistentStorage verifyStorage;
        services::PersistentStateService verifyState(verifyStorage);
        verifyState.begin();
        CHECK_EQ(verifyState.storedTransportState(), TransportState::Playing);
    }

    fakefw::writes.clear();
    for (std::uint32_t tick = 0U; tick < 25000U; ++tick) {
        HardwareTimer::lastInstance->fire();
    }
    for (const std::uint32_t gatePin : pinmap::kGateChannelPins) {
        CHECK_EQ(fakefw::pinValues[gatePin], LOW);
        const bool emittedHighGate = std::any_of(
            fakefw::writes.begin(),
            fakefw::writes.end(),
            [gatePin](const fakefw::PinWrite& write) {
                return write.pin == gatePin && write.value == HIGH;
            });
        CHECK(!emittedHighGate);
    }

    // Framework glue functions use the translation-unit static application.
    resetFakes(); prepareDisplaySuccess(); setup(); loop(); CHECK(HardwareTimer::lastInstance!=nullptr);
}

namespace {
bool gIsrArmedForNextRead = false;
std::uint8_t gIsrRunLevel = HIGH;

/** Simulates the scheduler ISR changing the RUN level in the middle of runOnce(). */
void injectRunLevelIsrDuringForeground() {
    if (!gIsrArmedForNextRead) {
        return;
    }
    gIsrArmedForNextRead = false;  // The ISR itself reads pins; never re-enter.
    fakefw::setPin(pinmap::kExternalSyncSignalPin, gIsrRunLevel);
    HardwareTimer::lastInstance->fire();
}

void runPasses(app::ClockApplication& application, const std::uint32_t untilMs) {
    while (fakefw::nowMs < untilMs) {
        fakefw::nowMs += 5U;
        application.runOnce();
    }
}

void pressStop(app::ClockApplication& application) {
    fakefw::setPin(pinmap::kResetBackButtonPin, LOW);
    runPasses(application, fakefw::nowMs + 60U);
    fakefw::setPin(pinmap::kResetBackButtonPin, HIGH);
    runPasses(application, fakefw::nowMs + 60U);
}
}  // namespace

void testFlashCommitRespectsEngineStartedByIsr() {
    // F-002: the commit gate must follow the engine, not the foreground transport
    // field, because the scheduler ISR can start the engine mid-pass.
    resetFakes();
    prepareDisplaySuccess();
    fakefw::delayAdvanceOverrideMs = 333U;
    fakefw::setPinSilently(pinmap::kExternalSyncSignalPin, LOW);
    ClockState initial = makeDefaultState();
    initial.transport = TransportState::Stopped;
    initial.inputs = {InputFunction::Run, InputFunction::Off};
    app::ClockApplication application;
    application.begin(initial);
    CHECK(HardwareTimer::lastInstance != nullptr);
    fakefw::digitalReadHook = injectRunLevelIsrDuringForeground;
    runPasses(application, fakefw::nowMs + 50U);
    CHECK(!application.enginePlayingForTest());

    // STOP queues a CURRENT/transport write that becomes due after the commit delay.
    pressStop(application);
    CHECK_EQ(application.transportForTest(), TransportState::Stopped);
    const std::uint32_t commitsBefore = hal::PersistentStorage::writeCommitCountForTest();
    fakefw::nowMs += config::kPersistenceCommitDelayMs + 10U;

    // RUN goes HIGH inside the pass that would flush the write.
    gIsrRunLevel = HIGH;
    gIsrArmedForNextRead = true;
    application.runOnce();
    CHECK(!gIsrArmedForNextRead);
    CHECK(application.enginePlayingForTest());
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), commitsBefore);

    // The queued PLAY transition is adopted on the next pass; still no commit.
    fakefw::nowMs += 5U;
    application.runOnce();
    CHECK_EQ(application.transportForTest(), TransportState::Playing);
    CHECK_EQ(hal::PersistentStorage::writeCommitCountForTest(), commitsBefore);

    // RUN LOW stops the engine; the deferred write is flushed afterwards.
    gIsrRunLevel = LOW;
    gIsrArmedForNextRead = true;
    fakefw::nowMs += 5U;
    application.runOnce();
    runPasses(application, fakefw::nowMs + 20U);
    CHECK(!application.enginePlayingForTest());
    CHECK_EQ(application.transportForTest(), TransportState::Stopped);
    runPasses(application, fakefw::nowMs + config::kPersistenceCommitDelayMs + 20U);
    CHECK(hal::PersistentStorage::writeCommitCountForTest() > commitsBefore);
    fakefw::digitalReadHook = nullptr;
}

void testStaleIsrTransitionDoesNotOverrideManualStop() {
    // F-026: RUN starts the engine at the start of a pass, STOP stops it in the
    // same pass. The queued PLAY transition is stale and must not show PLAY.
    resetFakes();
    prepareDisplaySuccess();
    fakefw::delayAdvanceOverrideMs = 333U;
    fakefw::setPinSilently(pinmap::kExternalSyncSignalPin, LOW);
    ClockState initial = makeDefaultState();
    initial.transport = TransportState::Stopped;
    initial.inputs = {InputFunction::Run, InputFunction::Off};
    app::ClockApplication application;
    application.begin(initial);
    fakefw::digitalReadHook = injectRunLevelIsrDuringForeground;
    runPasses(application, fakefw::nowMs + 50U);

    // Debounced STOP edge arrives in the same pass in which the ISR raises RUN.
    fakefw::setPin(pinmap::kResetBackButtonPin, LOW);
    fakefw::nowMs += 5U;
    application.runOnce();
    fakefw::nowMs += 30U;
    gIsrRunLevel = HIGH;
    gIsrArmedForNextRead = true;
    application.runOnce();
    CHECK(!gIsrArmedForNextRead);
    CHECK(!application.enginePlayingForTest());

    fakefw::nowMs += 5U;
    application.runOnce();
    CHECK(!application.enginePlayingForTest());
    CHECK_EQ(application.transportForTest(), TransportState::Stopped);
    fakefw::setPin(pinmap::kResetBackButtonPin, HIGH);
    fakefw::digitalReadHook = nullptr;
}

namespace {
constexpr std::uint32_t kTapPassMs = 5U;

/** Advances SysTick milliseconds and TIM5 microseconds independently, like the hardware. */
void runPassesOnBothClocks(app::ClockApplication& application, const std::uint32_t untilMs) {
    while (fakefw::nowMs < untilMs) {
        fakefw::nowMs += kTapPassMs;
        fakefw::nowUs += kTapPassMs * 1000U;
        HardwareTimer::lastInstance->fire();
        application.runOnce();
    }
}

/** Boots with INPUT 1 = TAP at 90 BPM and moves both clocks to the requested uptime. */
void bootTapApplication(
    app::ClockApplication& application,
    const std::uint32_t uptimeMs,
    const std::uint32_t timerUs) {
    resetFakes();
    prepareDisplaySuccess();
    fakefw::setPinSilently(pinmap::kExternalSyncSignalPin, LOW);
    ClockState initial = makeDefaultState();
    initial.transport = TransportState::Stopped;
    initial.bpm = 90U;
    initial.inputs = {InputFunction::Tap, InputFunction::Off};
    application.begin(initial);
    CHECK(HardwareTimer::lastInstance != nullptr);
    fakefw::nowMs = uptimeMs;
    fakefw::nowUs = timerUs;
    runPassesOnBothClocks(application, fakefw::nowMs + 50U);
    CHECK_EQ(application.bpmForTest(), 90U);
}

/** One 10-ms INPUT 1 pulse; the EXTI capture stamps the rising edge with TIM5 time. */
void inputTapPulse(app::ClockApplication& application) {
    fakefw::setPin(pinmap::kExternalSyncSignalPin, HIGH);
    runPassesOnBothClocks(application, fakefw::nowMs + 10U);
    fakefw::setPin(pinmap::kExternalSyncSignalPin, LOW);
}

/** Front-panel TAP whose 25-ms debounced release is recognized at @p releaseMs (+ one pass). */
void panelTapReleasedAt(app::ClockApplication& application, const std::uint32_t releaseMs) {
    fakefw::setPin(pinmap::kTapTempoButtonPin, LOW);
    runPassesOnBothClocks(application, releaseMs - 25U);
    fakefw::setPin(pinmap::kTapTempoButtonPin, HIGH);
}

/**
 * Four taps 500 ms apart starting at @p uptimeMs / @p timerUs. @p panelMask selects which
 * taps come from the front panel (bit n = tap n); the rest arrive on INPUT 1.
 */
std::uint16_t tempoAfterFourTaps(
    const std::uint32_t uptimeMs,
    const std::uint32_t timerUs,
    const std::uint8_t panelMask) {
    app::ClockApplication application;
    bootTapApplication(application, uptimeMs, timerUs);
    const std::uint32_t firstTapMs = fakefw::nowMs + 100U;
    for (std::uint32_t tap = 0U; tap < 4U; ++tap) {
        const std::uint32_t tapMs = firstTapMs + tap * 500U;
        if ((panelMask & (1U << tap)) != 0U) {
            runPassesOnBothClocks(application, tapMs - 100U);
            panelTapReleasedAt(application, tapMs);
            runPassesOnBothClocks(application, tapMs + 100U);
        } else {
            runPassesOnBothClocks(application, tapMs);
            inputTapPulse(application);
        }
    }
    runPassesOnBothClocks(application, fakefw::nowMs + 100U);
    return application.bpmForTest();
}
}  // namespace

/**
 * @brief Issue #8: external TAP must keep working after the 32-bit microsecond wrap.
 *
 * SysTick milliseconds and TIM5 microseconds run as independent counters. Before the
 * wrap (~71.6 min) both views agree; afterwards the input timestamp is far below the
 * millisecond time. Every case must still turn 90 BPM into 120 BPM.
 */
void testExternalTapTempoSurvivesMicrosecondWrap() {
    constexpr std::uint32_t kWrapUs = 0xFFFFF000U;
    // Before the wrap: both time bases agree.
    CHECK_EQ(tempoAfterFourTaps(10000U, 10000000U, 0x0U), 120U);
    // The issue's reproduction: uptime 4,296,000,000 us, just after the wrap.
    CHECK_EQ(tempoAfterFourTaps(4296000U, static_cast<std::uint32_t>(4296000000ULL), 0x0U), 120U);
    // Taps that straddle the wrap: the timer starts 4096 us before 2^32.
    CHECK_EQ(tempoAfterFourTaps(kWrapUs / 1000U, kWrapUs - 200000U, 0x0U), 120U);
    // Well after the wrap: 150 min uptime, more than two timer wraps.
    CHECK_EQ(tempoAfterFourTaps(9000000U, static_cast<std::uint32_t>(9000000000ULL), 0x0U), 120U);
}

/**
 * @brief Panel and INPUT 1 taps form one sequence in one time base, also after the wrap.
 *
 * The panel release is recognized within one 5-ms foreground pass, so the mixed tempo may
 * deviate by one BPM; it must be identical before and well after the timer wraps.
 */
void testMixedPanelAndInputTapsShareOneTimeBase() {
    for (const std::uint8_t mask :
         {std::uint8_t{0x5U}, std::uint8_t{0xAU}, std::uint8_t{0x3U}, std::uint8_t{0xFU}}) {
        // SysTick and TIM5 are independent counters: give them an arbitrary offset, as
        // after boot or after SysTick ticks lost during a Flash commit (F-046).
        constexpr std::uint32_t kTimerOffsetUs = 0x12345678U;
        const std::uint16_t beforeWrap = tempoAfterFourTaps(10000U, 10000000U + kTimerOffsetUs, mask);
        const std::uint16_t afterWrap = tempoAfterFourTaps(
            9000000U, static_cast<std::uint32_t>(9000000000ULL) + kTimerOffsetUs, mask);
        CHECK(beforeWrap >= 119U && beforeWrap <= 121U);
        CHECK_EQ(afterWrap, beforeWrap);
    }
}
