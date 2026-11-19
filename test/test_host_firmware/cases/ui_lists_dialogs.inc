/**
 * @file ui_lists_dialogs.inc
 * @brief List scroll windows, confirmation dialogs and list-state cases of the UI controller.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

namespace {

/** Returns whether one logical framebuffer pixel is lit. */
bool listPixelSet(const std::array<std::uint8_t, hal::OledDisplay::kFramebufferSize>& frame, const int x, const int y) {
    const std::size_t index = static_cast<std::size_t>(x) +
        static_cast<std::size_t>(y / 8) * static_cast<std::size_t>(hal::OledDisplay::kWidth);
    return (frame[index] & static_cast<std::uint8_t>(1U << (y & 7))) != 0U;
}

/**
 * Returns the only highlighted list row, -1 when no row is highlighted, -2 when several are.
 * A highlighted row is an inverted bar, probed at column @p probeX of each row's bar.
 */
int highlightedListRow(
    const hal::OledDisplay& display,
    const int probeX,
    const int firstBarY,
    const int pitch,
    const int rows) {
    int found = -1;
    for (int row = 0; row < rows; ++row) {
        if (listPixelSet(display.framebufferForTest(), probeX, firstBarY + row * pitch)) {
            if (found != -1) {
                return -2;
            }
            found = row;
        }
    }
    return found;
}

/** Controller, renderer and stores used by the list and dialog cases. */
struct ListRig {
    hal::OledDisplay display;
    ClockState state = makeDefaultState();
    hal::GateOutputDriver gates;
    engine::ClockEngine engine{gates};
    hal::PersistentStorage storage;
    services::PersistentStateService persistence{storage};
    services::CustomGrooveStore grooves{storage};
    ui::UiRenderer renderer{display, persistence, grooves};
    ui::UiController controller{state, engine, renderer, persistence, grooves};
    std::uint32_t now = 10U;

    ListRig() {
        gates.beginDisabled();
        engine.begin(state);
        persistence.begin();
    }

    ui::NavigationState& navigation() { return ui::UiControllerTestAccess::navigation(controller); }

    void render() { renderer.render(state, controller.navigation(), engine.snapshot()); }
};

/** Prepares the fakes before a ListRig is constructed. */
void prepareListRig() {
    resetFakes();
    prepareDisplaySuccess();
    hal::PersistentStorage::resetForTest();
}

}  // namespace

void testTemplateCursorStaysVisibleAtEveryPosition() {
    // F-019 / W-083: the controller scrolled with five rows while the renderer drew four, so
    // the cursor on template 5 or 6 was not visible.
    prepareListRig();
    ListRig rig;
    CHECK(rig.display.begin());
    rig.navigation().screen = ui::Screen::Templates;
    rig.navigation().cursor = 0U;
    rig.navigation().scrollOffset = 0U;
    constexpr int kTemplateBarY = 13;  // bar of row r spans y = 12 + 12 r ... 21 + 12 r
    constexpr int kTemplatePitch = 12;
    const int count = static_cast<int>(services::TemplateService::kTemplateCount);
    for (int position = 0; position < count; ++position) {
        if (position != 0) {
            controllerTurn(rig.controller, 1, rig.now);
        }
        CHECK_EQ(static_cast<int>(rig.controller.navigation().cursor), position);
        rig.render();
        const int row = highlightedListRow(rig.display, 122, kTemplateBarY, kTemplatePitch, 4);
        CHECK(row >= 0);
        CHECK_EQ(row, position - static_cast<int>(rig.controller.navigation().scrollOffset));
    }
    for (int position = count - 1; position >= 0; --position) {
        if (position != count - 1) {
            controllerTurn(rig.controller, -1, rig.now);
        }
        rig.render();
        const int row = highlightedListRow(rig.display, 122, kTemplateBarY, kTemplatePitch, 4);
        CHECK_EQ(row, position - static_cast<int>(rig.controller.navigation().scrollOffset));
    }
}

void testPresetAndGrooveSlotCursorsStayVisible() {
    // W-084: the two other "+ 5U" scroll limits match their five-row renderers.
    prepareListRig();
    ListRig rig;
    CHECK(rig.display.begin());
    constexpr int kSlotBarY = 12;  // bar of row r spans y = 11 + 10 r ... 19 + 10 r
    constexpr int kSlotPitch = 10;

    rig.navigation().screen = ui::Screen::PresetSlots;
    rig.navigation().presetSlotAction = ui::PresetSlotAction::Load;
    rig.navigation().cursor = 0U;
    rig.navigation().scrollOffset = 0U;
    const int presets = static_cast<int>(services::PersistentStateService::kUserPresetSlotCount);
    for (int position = 0; position < presets; ++position) {
        if (position != 0) {
            controllerTurn(rig.controller, 1, rig.now);
        }
        rig.render();
        CHECK_EQ(
            highlightedListRow(rig.display, 122, kSlotBarY, kSlotPitch, 5),
            position - static_cast<int>(rig.controller.navigation().scrollOffset));
    }

    rig.navigation().screen = ui::Screen::GrooveSlots;
    rig.navigation().grooveSlotAction = ui::GrooveSlotAction::Activate;
    rig.navigation().cursor = 0U;
    rig.navigation().scrollOffset = 0U;
    const int slots = static_cast<int>(kCustomGrooveSlotCount);
    for (int position = 0; position < slots; ++position) {
        if (position != 0) {
            controllerTurn(rig.controller, 1, rig.now);
        }
        rig.render();
        CHECK_EQ(
            highlightedListRow(rig.display, 122, kSlotBarY, kSlotPitch, 5),
            position - static_cast<int>(rig.controller.navigation().scrollOffset));
    }
}

void testTemplateApplyAsksWithNoPreselected() {
    // D-09 / W-086: APPLY TEMPLATE? with NO preselected, like CHANGE MODE.
    prepareListRig();
    ListRig rig;
    CHECK(rig.display.begin());
    rig.state.bpm = 133U;
    rig.state.channels[0].common.rate.factor = 3U;
    rig.navigation().screen = ui::Screen::Templates;
    rig.navigation().cursor = 0U;
    rig.navigation().scrollOffset = 0U;
    controllerTurn(rig.controller, 1, rig.now);  // CLOCK TREE

    controllerShortPress(rig.controller, rig.now);
    CHECK(rig.controller.navigation().screen == ui::Screen::TemplateApplyConfirm);
    CHECK_EQ(rig.controller.navigation().cursor, 0U);
    rig.render();
    const char* const title = text::get(text::TextId::ApplyTemplate);
    CHECK(std::strcmp(title, "APPLY TEMPLATE?") == 0);
    CHECK(rig.display.measureText(title, 0, 0).width <= hal::OledDisplay::kWidth - 4U);

    // NO keeps the configuration and returns to the highlighted template.
    controllerShortPress(rig.controller, rig.now);
    CHECK(rig.controller.navigation().screen == ui::Screen::Templates);
    CHECK_EQ(rig.controller.navigation().cursor, 1U);
    CHECK_EQ(rig.state.bpm, 133U);
    CHECK_EQ(rig.state.channels[0].common.rate.factor, 3U);

    // BACK cancels as well.
    controllerShortPress(rig.controller, rig.now);
    CHECK(rig.controller.navigation().screen == ui::Screen::TemplateApplyConfirm);
    controllerReset(rig.controller, rig.now);
    CHECK(rig.controller.navigation().screen == ui::Screen::Templates);
    CHECK_EQ(rig.controller.navigation().cursor, 1U);
    CHECK_EQ(rig.state.bpm, 133U);

    // YES applies the template.
    controllerShortPress(rig.controller, rig.now);
    controllerTurn(rig.controller, 1, rig.now);
    CHECK_EQ(rig.controller.navigation().cursor, 1U);
    rig.render();
    controllerShortPress(rig.controller, rig.now);
    CHECK(rig.controller.navigation().screen == ui::Screen::Performance);
    ClockState expected = makeDefaultState();
    CHECK(services::TemplateService::apply(1U, expected));
    CHECK_EQ(rig.state.bpm, expected.bpm);
    CHECK(rig.state.operatingMode == OperatingMode::Independent);
    for (std::size_t channel = 0U; channel < kChannelCount; ++channel) {
        CHECK(rig.state.channels[channel].common.rate.mode == expected.channels[channel].common.rate.mode);
        CHECK_EQ(rig.state.channels[channel].common.rate.factor, expected.channels[channel].common.rate.factor);
    }
}

namespace {

/** ListRig with the Sequencer 2.0 stores attached. */
struct SequencerListRig {
    hal::OledDisplay display;
    ClockState state = makeDefaultState();
    hal::GateOutputDriver gates;
    engine::ClockEngine engine{gates};
    hal::PersistentStorage storage;
    services::PersistentStateService persistence{storage};
    services::CustomGrooveStore grooves{storage};
    services::SequencerPatternStore patterns{storage};
    services::SequencerStepStore steps{storage};
    ui::UiRenderer renderer{display, persistence, grooves, patterns, steps};
    ui::UiController controller{state, engine, renderer, persistence, grooves, patterns, steps};
    std::uint32_t now = 10U;

    SequencerListRig() {
        gates.beginDisabled();
        state.operatingMode = OperatingMode::Independent;
        state.channels[0].common.mode = ChannelMode::Sequencer;
        engine.begin(state);
        persistence.begin();
        CHECK(patterns.begin());
        CHECK(steps.begin());
    }

    ui::NavigationState& navigation() { return ui::UiControllerTestAccess::navigation(controller); }

    void setActiveLength(const std::uint8_t length) {
        SequencerPatternV2 pattern = patterns.pattern(0U, patterns.activeSlot(0U));
        pattern.length = length;
        CHECK(patterns.updatePattern(0U, patterns.activeSlot(0U), pattern, now++));
    }
};

}  // namespace

void testPerformanceScreenShowsTheActivePatternLength() {
    // F-035 / W-091: the Sequencer length on the Performance screen comes from the active
    // Sequencer 2.0 pattern, not from the legacy configuration field.
    prepareListRig();
    hal::OledDisplay display;
    CHECK(display.begin());
    ui::PerformanceRenderer renderer(display);
    ClockState state = makeDefaultState();
    state.operatingMode = OperatingMode::Independent;
    state.channels[0].common.mode = ChannelMode::Sequencer;
    ui::NavigationState nav{};
    nav.screen = ui::Screen::Performance;
    nav.selectedChannel = 0U;
    hal::GateOutputDriver gates;
    gates.beginDisabled();
    engine::ClockEngine engine(gates);
    engine.begin(state);
    SequencerPatternV2 active{};
    active.length = 7U;

    state.channels[0].sequencer.length = 16U;
    renderer.render(state, nav, engine.snapshot(), &active);
    const auto withStaleLegacyLength = display.framebufferForTest();
    state.channels[0].sequencer.length = 7U;
    renderer.render(state, nav, engine.snapshot(), &active);
    CHECK(display.framebufferForTest() == withStaleLegacyLength);
}

void testSequencerCursorIsClampedAfterAShorterPaste() {
    // F-036 / W-092: after PASTE of a shorter pattern the cursor is on the last step.
    prepareListRig();
    SequencerListRig rig;
    CHECK(rig.display.begin());
    auto& nav = rig.navigation();
    nav.screen = ui::Screen::SequencerEditor;
    nav.selectedChannel = 0U;

    rig.setActiveLength(10U);
    CHECK(ui::UiControllerTestAccess::command(rig.controller, 3U, rig.now++));  // COPY
    rig.setActiveLength(16U);
    nav.sequencerPage = 1U;
    nav.sequencerCursor = 6U;  // step 15
    CHECK(ui::UiControllerTestAccess::command(rig.controller, 4U, rig.now++));  // PASTE
    CHECK_EQ(rig.patterns.pattern(0U, rig.patterns.activeSlot(0U)).length, 10U);
    CHECK_EQ(nav.sequencerPage * 8U + nav.sequencerCursor, 9U);

    rig.setActiveLength(5U);
    CHECK(ui::UiControllerTestAccess::command(rig.controller, 3U, rig.now++));  // COPY
    rig.setActiveLength(16U);
    nav.sequencerPage = 1U;
    nav.sequencerCursor = 6U;
    CHECK(ui::UiControllerTestAccess::command(rig.controller, 4U, rig.now++));  // PASTE
    CHECK_EQ(nav.sequencerPage, 0U);
    CHECK_EQ(nav.sequencerCursor, 4U);
}

void testCustomGrooveIsOfferedOnlyWithAStoredGroove() {
    // F-037 / W-094 / E-13: CUSTOM is offered only when a Custom Groove slot holds a groove.
    prepareListRig();
    ListRig rig;
    CHECK(rig.display.begin());
    rig.state.operatingMode = OperatingMode::Independent;
    rig.state.channels[0].common.groove.preset = GroovePreset::PocketC;
    auto& nav = rig.navigation();
    nav.screen = ui::Screen::Settings;
    nav.settingsPage = ui::SettingsPage::Groove;
    nav.selectedChannel = 0U;
    nav.cursor = 0U;  // PRESET
    nav.editing = true;

    // All slots empty: the list ends at the last factory groove.
    controllerTurn(rig.controller, 1, rig.now);
    CHECK(rig.state.channels[0].common.groove.preset == GroovePreset::PocketC);

    // One slot used: CUSTOM is offered and refers to that slot.
    CustomGroovePattern stored{};
    stored.length = 8U;
    stored.offsets256[1] = 12;
    CHECK(rig.grooves.save(2U, "SLOT THREE", stored));
    controllerTurn(rig.controller, 1, rig.now);
    CHECK(rig.state.channels[0].common.groove.preset == GroovePreset::Custom);
    CHECK_EQ(rig.state.channels[0].common.groove.customSlot, 2U);
    controllerTurn(rig.controller, -1, rig.now);
    CHECK(rig.state.channels[0].common.groove.preset == GroovePreset::PocketC);

    // The slot is cleared while CUSTOM is selected: the channel falls back to OFF.
    controllerTurn(rig.controller, 1, rig.now);
    CHECK(rig.state.channels[0].common.groove.preset == GroovePreset::Custom);
    nav.editing = false;
    nav.screen = ui::Screen::GrooveDeleteConfirm;
    nav.selectedGrooveSlot = 2U;
    nav.cursor = 1U;  // YES
    controllerShortPress(rig.controller, rig.now);
    CHECK(!rig.grooves.exists(2U));
    CHECK(rig.state.channels[0].common.groove.preset == GroovePreset::Off);
    nav.screen = ui::Screen::Settings;
    nav.settingsPage = ui::SettingsPage::Groove;
    nav.cursor = 0U;
    nav.editing = true;
    controllerTurn(rig.controller, 20, rig.now);
    CHECK(rig.state.channels[0].common.groove.preset == GroovePreset::PocketC);
}

void testHighScoreClearReturnsToTheHighScoresRow() {
    // F-038 / W-095: after the HI-SCORES dialog the cursor is on HI-SCORES, not on INFO.
    prepareListRig();
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
    game::ArcadeLeaderboardStore leaderboard(storage, game::ArcadeGameId::PixelRaid);
    CHECK(leaderboard.markStarted());
    ui::UiRenderer renderer(display, persistence);
    ui::UiController controller(state, engine, renderer, persistence, &leaderboard);
    std::uint32_t now = 100U;

    controllerOpenSettingsChord(controller, now);
    CHECK(controller.navigation().highScoreResetAvailable);
    controllerTurn(controller, 5, now);
    CHECK_EQ(controller.navigation().cursor, 5U);
    controllerShortPress(controller, now);
    CHECK(controller.navigation().screen == ui::Screen::HighScoreClearConfirm);
    controllerShortPress(controller, now);  // NO
    CHECK(controller.navigation().settingsPage == ui::SettingsPage::Root);
    CHECK_EQ(controller.navigation().cursor, 5U);

    controllerShortPress(controller, now);
    controllerReset(controller, now);  // BACK
    CHECK_EQ(controller.navigation().cursor, 5U);

    controllerShortPress(controller, now);
    controllerTurn(controller, 1, now);  // YES
    controllerShortPress(controller, now);
    CHECK(controller.navigation().highScoreResetAvailable);
    CHECK_EQ(controller.navigation().cursor, 5U);
    const ui::MenuRow row = ui::buildMenuRow(
        ui::SettingsPage::Root, controller.navigation().cursor, 0U, state, true);
    CHECK(std::strcmp(row.label, text::get(text::TextId::HighScores)) == 0);
}

void testGrooveZoomFlagEndsWithTheScreen() {
    // F-039 / W-096: PLAY held for zoom in the editor, then BACK: the next PLAY in the
    // recorder starts recording.
    prepareListRig();
    ListRig rig;
    CHECK(rig.display.begin());
    auto& nav = rig.navigation();
    nav.screen = ui::Screen::GrooveEditor;
    nav.grooveDraft.length = 8U;
    hal::ControlSample sample{};
    sample.transportButton = pressedEdge();
    rig.controller.processControls(sample, rig.now++);
    sample.transportButton = heldButton();
    sample.encoderDelta = 1;  // zoom while PLAY is held
    rig.controller.processControls(sample, rig.now++);
    sample.encoderDelta = 0;
    sample.resetButton = pressedEdge();  // BACK leaves the editor while PLAY is held
    rig.controller.processControls(sample, rig.now++);
    CHECK(rig.controller.navigation().screen != ui::Screen::GrooveEditor);
    sample = {};
    sample.transportButton = releasedEdge();
    rig.controller.processControls(sample, rig.now++);

    nav.screen = ui::Screen::GrooveRecorder;
    nav.grooveRecordCountInBeats = 0U;
    sample = {};
    sample.transportButton = pressedEdge();
    rig.controller.processControls(sample, rig.now++);
    sample.transportButton = releasedEdge();
    rig.controller.processControls(sample, rig.now++);
    CHECK(rig.controller.navigation().grooveRecordState == ui::GrooveRecordState::Recording);
}

void testInformationPopoverWrapsAtWordBoundaries() {
    // F-049 / W-300: break at spaces and after "/", hard break only inside longer words.
    prepareListRig();
    ListRig rig;
    CHECK(rig.display.begin());
    hal::OledDisplay expected;
    const auto popover = [&](const char* const value) {
        auto& nav = rig.navigation();
        nav.screen = ui::Screen::InformationPopover;
        std::snprintf(nav.informationPopoverTitle.data(), nav.informationPopoverTitle.size(), "%s", "KO-FI");
        std::snprintf(nav.informationPopoverValue.data(), nav.informationPopoverValue.size(), "%s", value);
        rig.render();
    };
    const auto twoLines = [&](const char* const first, const char* const second) {
        expected.clear();
        expected.setFont(hal::DisplayFont::Small);
        expected.setTextColor(hal::PixelColor::White);
        expected.drawText(0, 0, "KO-FI");
        expected.drawHorizontalLine(0, 9, hal::OledDisplay::kWidth);
        const std::array<const char*, 2U> lines{{first, second}};
        constexpr std::array<std::int16_t, 2U> kLineY{{22, 34}};
        for (std::size_t index = 0U; index < lines.size(); ++index) {
            const hal::TextBounds bounds = expected.measureText(lines[index], 0, 0);
            expected.drawText(
                static_cast<std::int16_t>((static_cast<int>(hal::OledDisplay::kWidth) - static_cast<int>(bounds.width)) / 2),
                kLineY[index],
                lines[index]);
        }
        return expected.framebufferForTest();
    };

    popover(text::get(text::TextId::KofiUrl));
    CHECK(rig.display.framebufferForTest() == twoLines("ko-fi.com/", "southsignallab"));
    const std::string words = "SOUTH SIGNAL LAB CLOCK FIRMWARE";
    std::size_t lastSpace = 0U;
    for (std::size_t index = 1U; index < words.size(); ++index) {
        if (words[index] == ' ' && expected.measureText(words.substr(0U, index).c_str(), 0, 0).width <= 124U) {
            lastSpace = index;
        }
    }
    CHECK(lastSpace > 0U);
    popover(words.c_str());
    CHECK(rig.display.framebufferForTest() ==
        twoLines(words.substr(0U, lastSpace).c_str(), words.substr(lastSpace + 1U).c_str()));
    popover("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123");
    const std::string hard = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123";
    std::size_t fit = hard.size();
    while (expected.measureText(hard.substr(0U, fit).c_str(), 0, 0).width > 124U) {
        --fit;
    }
    CHECK(rig.display.framebufferForTest() ==
        twoLines(hard.substr(0U, fit).c_str(), hard.substr(fit).c_str()));
}
