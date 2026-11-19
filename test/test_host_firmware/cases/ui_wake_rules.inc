/**
 * @file ui_wake_rules.inc
 * @brief Display wake rules (D-08, E-11) and the TAP modifier of the UI controller.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

namespace {

constexpr std::uint32_t kWakePassMs = 5U;

/** Advances both clocks by one foreground pass, runs one scheduler tick and one pass. */
void wakePass(app::ClockApplication& application) {
    fakefw::nowMs += kWakePassMs;
    fakefw::nowUs += kWakePassMs * 1000U;
    HardwareTimer::lastInstance->fire();
    application.runOnce();
}

/** Runs passes until @p untilMs; IN 1 carries a 50 Hz clock (24 PPQN at 125 BPM) when @p clock. */
void wakePassesWithClock(app::ClockApplication& application, const std::uint32_t untilMs, const bool clock) {
    std::uint32_t pass = 0U;
    while (fakefw::nowMs < untilMs) {
        if (clock && (pass % 2U) == 0U) {
            fakefw::setPin(pinmap::kExternalSyncSignalPin, (pass % 4U) == 0U ? HIGH : LOW);
        }
        wakePass(application);
        ++pass;
    }
}

/** One 10-ms pulse on IN 2. */
void inputTwoPulse(app::ClockApplication& application) {
    fakefw::setPin(pinmap::kExternalResetSignalPin, HIGH);
    wakePassesWithClock(application, fakefw::nowMs + 10U, false);
    fakefw::setPin(pinmap::kExternalResetSignalPin, LOW);
    wakePassesWithClock(application, fakefw::nowMs + 20U, false);
}

/** Boots STOPPED with IN 1 = SYNC (24 PPQN) and IN 2 = @p inputTwo, factory display timers. */
void bootWakeApplication(app::ClockApplication& application, const InputFunction inputTwo) {
    resetFakes();
    prepareDisplaySuccess();
    fakefw::setPinSilently(pinmap::kExternalSyncSignalPin, LOW);
    fakefw::setPinSilently(pinmap::kExternalResetSignalPin, LOW);
    ClockState initial = makeDefaultState();
    initial.transport = TransportState::Stopped;
    initial.inputs = {InputFunction::Sync, inputTwo};
    initial.externalSync.pulsesPerQuarterNote = 24U;
    initial.display = {ScreensaverMode::Clock, 2U, 5U, 10U};
    application.begin(initial);
    CHECK(HardwareTimer::lastInstance != nullptr);
    wakePassesWithClock(application, fakefw::nowMs + 50U, false);
}

/** Controller with the display already switched off in STOP (1/2/3 minute timers). */
struct SleepingUiRig {
    hal::OledDisplay display;
    ClockState state = makeDefaultState();
    hal::GateOutputDriver gates;
    engine::ClockEngine engine{gates};
    hal::PersistentStorage storage;
    services::PersistentStateService persistence{storage};
    ui::UiRenderer renderer{display, persistence};
    ui::UiController controller{state, engine, renderer, persistence};
    std::uint32_t now = 0U;

    SleepingUiRig() {
        gates.beginDisabled();
        state.display = {ScreensaverMode::None, 1U, 2U, 3U};
        state.bpm = 100U;
        engine.begin(state);
        persistence.begin();
    }

    /** Puts the OLED to sleep: three idle minutes in STOP. */
    void sleep() {
        hal::ControlSample idle{};
        controller.processControls(idle, now);
        controller.serviceRendering(now + 1U);
        now += 180'001U;
        controller.serviceRendering(now);
        CHECK(controller.displayPoweredOffForTest());
    }

    void sample(const hal::ControlSample& controls) {
        ++now;
        controller.processControls(controls, now);
        controller.serviceRendering(now);
    }

    ui::NavigationState& navigation() { return ui::UiControllerTestAccess::navigation(controller); }
};

void prepareSleepingRig() {
    resetFakes();
    prepareDisplaySuccess();
    hal::PersistentStorage::resetForTest();
}

}  // namespace

void testSyncClockDoesNotKeepTheDisplayAwake() {
    // F-017 / W-082 / E-11: STOP with a 24 PPQN clock at SYNC; after 11 minutes the OLED is off.
    app::ClockApplication application;
    bootWakeApplication(application, InputFunction::Reset);
    wakePassesWithClock(application, fakefw::nowMs + 11U * 60'000U, true);
    CHECK(!application.enginePlayingForTest());
    CHECK(application.displayPoweredOffForTest());

    // A RESET trigger on IN 2 is not user activity either.
    inputTwoPulse(application);
    CHECK(application.displayPoweredOffForTest());
}

void testStartAndStopFromAnInputWakeTheDisplay() {
    // E-11 / W-088: a START pulse at an input wakes the display and starts the clock.
    app::ClockApplication application;
    bootWakeApplication(application, InputFunction::Start);
    wakePassesWithClock(application, fakefw::nowMs + 11U * 60'000U, false);
    CHECK(application.displayPoweredOffForTest());
    inputTwoPulse(application);
    CHECK(!application.displayPoweredOffForTest());
    CHECK(application.enginePlayingForTest());

    // A STOP pulse at an input wakes the stopped module as well.
    app::ClockApplication stopApplication;
    bootWakeApplication(stopApplication, InputFunction::Stop);
    wakePassesWithClock(stopApplication, fakefw::nowMs + 11U * 60'000U, false);
    CHECK(stopApplication.displayPoweredOffForTest());
    inputTwoPulse(stopApplication);
    CHECK(!stopApplication.displayPoweredOffForTest());
    CHECK(!stopApplication.enginePlayingForTest());
}

void testEncoderOnlyWakesTheSleepingDisplay() {
    // F-032 / D-08: with the OLED off, turning and pushing the encoder only wake it.
    prepareSleepingRig();
    SleepingUiRig rig;
    CHECK(rig.display.begin());
    rig.sleep();
    hal::ControlSample turn{};
    turn.encoderDelta = 3;
    rig.sample(turn);
    CHECK(!rig.controller.displayPoweredOffForTest());
    CHECK_EQ(rig.state.bpm, 100U);
    rig.sample(turn);  // awake: the next turn changes the tempo
    CHECK_EQ(rig.state.bpm, 103U);

    rig.sleep();
    hal::ControlSample push{};
    push.encoderButton = pressedEdge();
    rig.sample(push);
    CHECK(!rig.controller.displayPoweredOffForTest());
    hal::ControlSample held{};
    held.encoderButton = heldButton();
    rig.sample(held);
    push.encoderButton = releasedEdge();
    rig.sample(push);  // the release of the waking press does not act either
    CHECK(rig.controller.navigation().screen == ui::Screen::Performance);
    controllerShortPress(rig.controller, rig.now);  // awake: a push opens the channel list
    CHECK(rig.controller.navigation().screen == ui::Screen::ChannelQuickSelect);
}

void testMenuKeysOnlyWakeTheSleepingDisplay() {
    // D-08: TAP and BACK (menu keys) only wake.
    prepareSleepingRig();
    SleepingUiRig rig;
    CHECK(rig.display.begin());
    rig.sleep();
    // TAP pressed and released while asleep: no tap. A tap 500 ms later is then the first
    // tap of a sequence and does not change the tempo.
    hal::ControlSample tap{};
    tap.tapButton = pressedEdge();
    rig.sample(tap);
    tap.tapButton = releasedEdge();
    rig.sample(tap);
    CHECK(!rig.controller.displayPoweredOffForTest());
    rig.now += 500U;
    tap.tapButton = pressedEdge();
    rig.sample(tap);
    tap.tapButton = releasedEdge();
    rig.sample(tap);
    CHECK_EQ(rig.state.bpm, 100U);

    // BACK in a menu: wakes and stays on the page.
    controllerOpenSettingsChord(rig.controller, rig.now);
    CHECK(rig.controller.navigation().screen == ui::Screen::Settings);
    rig.sleep();
    hal::ControlSample back{};
    back.resetButton = pressedEdge();
    rig.sample(back);
    CHECK(!rig.controller.displayPoweredOffForTest());
    CHECK(rig.controller.navigation().screen == ui::Screen::Settings);
    back.resetButton = releasedEdge();
    rig.sample(back);
    CHECK(rig.controller.navigation().screen == ui::Screen::Settings);
}

void testTransportKeysActWhileTheDisplaySleeps() {
    // D-08: START/PAUSE and STOP act immediately and wake the display.
    prepareSleepingRig();
    SleepingUiRig rig;
    CHECK(rig.display.begin());
    rig.sleep();
    const std::uint32_t resetsBefore = rig.engine.resetRuntimeCountForTest();
    hal::ControlSample stop{};
    stop.resetButton = pressedEdge();
    rig.sample(stop);
    CHECK(!rig.controller.displayPoweredOffForTest());
    CHECK(rig.engine.resetRuntimeCountForTest() > resetsBefore);  // STOP acted (phase reset)
    stop.resetButton = releasedEdge();
    rig.sample(stop);

    rig.sleep();
    hal::ControlSample play{};
    play.transportButton = pressedEdge();
    rig.sample(play);
    CHECK(!rig.controller.displayPoweredOffForTest());
    CHECK(rig.state.transport == TransportState::Playing);
    CHECK(rig.engine.isPlaying());
}

void testTapReleaseAfterTapTurnDoesNotTriggerTheTapAction() {
    // F-033 / W-089: TAP held as a modifier while turning; its release must not fire TAP.
    prepareSleepingRig();
    SleepingUiRig rig;
    CHECK(rig.display.begin());
    auto& nav = rig.navigation();
    nav.screen = ui::Screen::GrooveEditor;
    nav.grooveDraft.length = 16U;
    nav.grooveCursor = 2U;
    hal::ControlSample sample{};
    sample.tapButton = pressedEdge();
    rig.sample(sample);
    sample.tapButton = heldButton();
    sample.encoderDelta = 1;  // coarse offset of the selected step
    rig.sample(sample);
    sample.encoderDelta = 0;
    sample.tapButton = releasedEdge();
    rig.sample(sample);
    CHECK_EQ(rig.controller.navigation().grooveCursor, 2U);
    // A plain TAP still moves the cursor.
    sample.tapButton = pressedEdge();
    rig.sample(sample);
    sample.tapButton = releasedEdge();
    rig.sample(sample);
    CHECK_EQ(rig.controller.navigation().grooveCursor, 3U);

    // Performance: TAP + turn changes the tempo; the release is not a tap.
    nav.screen = ui::Screen::Performance;
    rig.state.bpm = 100U;
    sample.tapButton = pressedEdge();
    rig.sample(sample);
    sample.tapButton = releasedEdge();
    rig.sample(sample);  // first tap of a sequence
    rig.now += 499U;
    sample.tapButton = pressedEdge();
    rig.sample(sample);
    sample.tapButton = heldButton();
    sample.encoderDelta = 2;
    rig.sample(sample);
    sample.encoderDelta = 0;
    sample.tapButton = releasedEdge();
    rig.sample(sample);
    CHECK_EQ(rig.state.bpm, 102U);  // the turn, not a 500 ms tap interval (120 BPM)
}
