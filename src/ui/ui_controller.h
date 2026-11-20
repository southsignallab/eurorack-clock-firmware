/**
 * @file ui_controller.h
 * @brief Navigation, gesture handling, and render invalidation for the OLED user interface.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstdint>

#include "domain/clock_types.h"
#include "engine/clock_engine.h"
#include "game/arcade_leaderboard_store.h"
#include "hal/control_panel.h"
#include "hal/external_input_capture.h"
#include "hal/gate_output_driver.h"
#include "services/custom_groove_store.h"
#include "services/groove_recorder.h"
#include "services/persistent_state_service.h"
#include "services/sequencer_pattern_store.h"
#include "services/sequencer_step_store.h"
#include "services/tap_tempo.h"
#include "ui/settings_editor.h"
#include "ui/ui_renderer.h"
#include "ui/ui_types.h"

namespace clockfw::ui {

/**
 * @brief Owns UI navigation, button semantics, screen transitions, and render invalidation.
 *
 * This class contains no GPIO or display-driver calls. Physical inputs arrive as
 * debounced HAL samples, validated value mutation is delegated to SettingsEditor,
 * persistence is delegated to PersistentStateService, and drawing to UiRenderer.
 */
class UiController final {
  public:
    /**
     * @brief Constructs the controller around application state and collaborators.
     * @param state Mutable musical/application state.
     * @param engine Real-time timing engine receiving committed configuration changes.
     * @param renderer Stateless screen renderer.
     * @param persistentState Wear-aware persistence service for user transport changes.
     * @param leaderboard Optional ranked Easter-egg leaderboard exposed for user reset.
     * @param externalInputs Optional external-input diagnostics provider.
     * @param gateOutputs Optional gate-output diagnostics provider.
     */
    UiController(
        ClockState& state,
        engine::ClockEngine& engine,
        UiRenderer& renderer,
        services::PersistentStateService& persistentState,
        game::ArcadeLeaderboardStore* leaderboard = nullptr,
        const hal::ExternalInputCapture* externalInputs = nullptr,
        const hal::GateOutputDriver* gateOutputs = nullptr);

    /**
     * @brief Constructs the controller with Custom Groove persistence enabled.
     * @param state Mutable musical/application state.
     * @param engine Real-time timing engine receiving committed configuration changes.
     * @param renderer Stateless screen renderer.
     * @param persistentState Wear-aware persistence service for user transport changes.
     * @param customGrooveStore Fixed-slot Custom Groove persistence service.
     * @param leaderboard Optional ranked Easter-egg leaderboard exposed for user reset.
     * @param externalInputs Optional external-input diagnostics provider.
     * @param gateOutputs Optional gate-output diagnostics provider.
     */
    UiController(
        ClockState& state,
        engine::ClockEngine& engine,
        UiRenderer& renderer,
        services::PersistentStateService& persistentState,
        services::CustomGrooveStore& customGrooveStore,
        game::ArcadeLeaderboardStore* leaderboard = nullptr,
        const hal::ExternalInputCapture* externalInputs = nullptr,
        const hal::GateOutputDriver* gateOutputs = nullptr);

    /**
     * @brief Constructs the controller with Custom Groove and Sequencer 2.0 persistence enabled.
     * @param state Mutable musical/application state.
     * @param engine Real-time timing engine receiving committed configuration changes.
     * @param renderer Stateless screen renderer.
     * @param persistentState Wear-aware persistence service for user transport changes.
     * @param customGrooveStore Fixed-slot Custom Groove persistence service.
     * @param sequencerPatternStore Sequencer 2.0 pattern persistence service.
     * @param leaderboard Optional ranked Easter-egg leaderboard exposed for user reset.
     * @param externalInputs Optional external-input diagnostics provider.
     * @param gateOutputs Optional gate-output diagnostics provider.
     */
    UiController(
        ClockState& state,
        engine::ClockEngine& engine,
        UiRenderer& renderer,
        services::PersistentStateService& persistentState,
        services::CustomGrooveStore& customGrooveStore,
        services::SequencerPatternStore& sequencerPatternStore,
        game::ArcadeLeaderboardStore* leaderboard = nullptr,
        const hal::ExternalInputCapture* externalInputs = nullptr,
        const hal::GateOutputDriver* gateOutputs = nullptr);

    /**
     * @brief Constructs the controller with persistent Sequencer 2.0 step expression enabled.
     * @param state Mutable musical/application state.
     * @param engine Real-time timing engine receiving committed configuration changes.
     * @param renderer Stateless screen renderer.
     * @param persistentState Wear-aware persistence service for user transport changes.
     * @param customGrooveStore Fixed-slot Custom Groove persistence service.
     * @param sequencerPatternStore Sequencer 2.0 pattern persistence service.
     * @param sequencerStepStore Sequencer 2.0 per-step expression persistence service.
     * @param leaderboard Optional ranked Easter-egg leaderboard exposed for user reset.
     * @param externalInputs Optional external-input diagnostics provider.
     * @param gateOutputs Optional gate-output diagnostics provider.
     */
    UiController(
        ClockState& state,
        engine::ClockEngine& engine,
        UiRenderer& renderer,
        services::PersistentStateService& persistentState,
        services::CustomGrooveStore& customGrooveStore,
        services::SequencerPatternStore& sequencerPatternStore,
        services::SequencerStepStore& sequencerStepStore,
        game::ArcadeLeaderboardStore* leaderboard = nullptr,
        const hal::ExternalInputCapture* externalInputs = nullptr,
        const hal::GateOutputDriver* gateOutputs = nullptr);

    /** @brief Marks the current frame as requiring a redraw. */
    void invalidate();

    /**
     * @brief Processes one debounced physical-control sample.
     * @param controls Current HAL control sample.
     * @param nowMs Current monotonic time in milliseconds.
     * @param nowUs Current TIM5 microseconds of the same time base; 0 is a valid timestamp.
     */
    void processControls(
        const hal::ControlSample& controls,
        std::uint32_t nowMs,
        std::uint32_t nowUs);

#ifdef CLOCK_HOST_TEST
    /**
     * @brief Host-test shorthand: microseconds derived from @p nowMs, one time base.
     * @param controls Current HAL control sample.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void processControls(const hal::ControlSample& controls, std::uint32_t nowMs);

    /** @brief True while STOP-mode display protection has switched the OLED off (host tests). */
    bool displayPoweredOffForTest() const {
        return displayPoweredOff_;
    }
#endif

    /**
     * @brief Services render invalidation and sends a frame when the refresh limit allows it.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void serviceRendering(std::uint32_t nowMs);

    /**
     * @brief Returns current navigation state for diagnostics and tests.
     * @return Reference to the controller-owned navigation state.
     */
    const NavigationState& navigation() const;

    /**
     * @brief Feeds an externally captured TAP edge into the normal Tap Tempo estimator.
     * @param timestampUs Capture time of the edge in the wrapping TIM5 microsecond time base.
     * @param nowMs Current monotonic time in milliseconds, for display and persistence timing.
     */
    void registerExternalTapTempo(std::uint32_t timestampUs, std::uint32_t nowMs);

    /**
     * @brief Records a START, STOP, RESTART or RUN command from an input as user activity.
     * @param nowMs Current monotonic time in milliseconds.
     *
     * Wakes the display and restarts the STOP-mode inactivity timers (E-11). SYNC clocks and
     * RESET triggers deliberately do not reach this function.
     */
    void noteExternalTransportCommand(std::uint32_t nowMs);

    /**
     * @brief Shows SAVE ERROR or UPDATE FIRMWARE on the Performance screen for a few seconds.
     * @param notice Notice to show; it is cleared after config::kPersistenceNoticeMs.
     * @param nowMs Current monotonic time in milliseconds; starts the display time.
     */
    void showPersistenceNotice(PersistenceNotice notice, std::uint32_t nowMs);

    /**
     * @brief Increments with every control sample that carries a front-panel action.
     * @return Wrapping count of control samples with user activity.
     */
    std::uint32_t controlActivityRevision() const {
        return controlActivityRevision_;
    }

    /**
     * @brief Seeds pseudo-random human-readable default names for this boot session.
     * @param seed Name generator seed; 0 selects the built-in default seed.
     */
    void seedGeneratedNames(std::uint32_t seed);

#ifdef CLOCK_SIMULATOR
    /** @brief Opens the existing General Settings page for a desktop host shell. */
    void openGeneralSettingsForHost();
#endif

  private:
    friend struct UiControllerTestAccess;

    /**
     * @brief Applies the D-08 wake rules to one control sample.
     * @param rawControls Sample from the control panel.
     * @return The sample to act on: while the OLED is off, the encoder and the menu keys
     *         only wake it, and a waking press stays hidden until its release.
     */
    hal::ControlSample applyWakeRules(const hal::ControlSample& rawControls);

    /**
     * @brief Returns true when one debounced front-panel sample represents user activity.
     * @param controls Debounced control sample.
     * @return true for an encoder detent or any button edge; a held press alone is no activity.
     */
    static bool hasUserActivity(const hal::ControlSample& controls);

    /**
     * @brief Routes one encoder detent according to the active screen and edit state.
     * @param delta Signed detent count of this sample.
     * @param tapPressed TAP is held as a modifier.
     * @param transportPressed PLAY is held as a modifier.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void handleEncoderDelta(
        std::int8_t delta,
        bool tapPressed,
        bool transportPressed,
        std::uint32_t nowMs);

    /**
     * @brief Applies one detent on the groove screens (editor, recorder, slots, names, dialogs).
     * @param delta Signed detent count.
     * @param tapPressed TAP is held: coarse marker offset in the Groove editor.
     * @param transportPressed PLAY is held: zoom instead of offset; the PLAY release is then
     *        consumed.
     * @return false when the active screen belongs to another group.
     */
    bool handleGrooveScreenDelta(std::int8_t delta, bool tapPressed, bool transportPressed);

    /**
     * @brief Applies one detent on channel quick select, performance tempo and mode select.
     * @param delta Signed detent count.
     * @param tapPressed TAP is held as a modifier (TAP-Turn).
     * @param nowMs Current monotonic time in milliseconds.
     * @return false when the active screen belongs to another group.
     */
    bool handleChannelScreenDelta(std::int8_t delta, bool tapPressed, std::uint32_t nowMs);

    /**
     * @brief Applies one detent on templates, preset slots, preset names and confirmation dialogs.
     * @param delta Signed detent count.
     * @return false when the active screen belongs to another group.
     */
    bool handlePresetScreenDelta(std::int8_t delta);

    /**
     * @brief Applies one detent on the settings menu: cursor movement and value editing.
     * @param delta Signed detent count.
     * @param nowMs Current monotonic time in milliseconds.
     * @return false when the active screen belongs to another group.
     */
    bool handleSettingsScreenDelta(std::int8_t delta, std::uint32_t nowMs);

    /**
     * @brief Applies one detent on the sequencer step editor and step grid.
     * @param delta Signed detent count.
     * @param nowMs Current monotonic time in milliseconds.
     * @return false when the active screen belongs to another group.
     */
    bool handleSequencerScreenDelta(std::int8_t delta, std::uint32_t nowMs);

    /**
     * @brief Applies one detent to the edited row of the Groove editor menu (zoom, length).
     * @param delta Signed detent count.
     */
    void adjustGrooveEditorMenuValue(std::int8_t delta);

    /**
     * @brief Applies one detent to the edited row of the Groove record menu.
     * @param delta Signed detent count.
     */
    void adjustGrooveRecordMenuValue(std::int8_t delta);

    /**
     * @brief Handles encoder push/release events.
     * @param button Debounced encoder push-button sample.
     * @param nowMs Current monotonic time in milliseconds; times the long press.
     */
    void handleEncoderButton(const hal::ButtonSample& button, std::uint32_t nowMs);

    /**
     * @brief Handles the transport button on performance and sequencer-editor screens.
     * @param button Debounced PLAY button sample.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void handleTransportButton(const hal::ButtonSample& button, std::uint32_t nowMs);

    /**
     * @brief Handles tap tempo, overview TAP-Turn mode selection, and sequencer page-back action.
     * @param button Debounced TAP button sample.
     * @param nowMs Current monotonic time in milliseconds.
     * @param nowUs Current time in the wrapping TIM5 microsecond time base; used when the
     *        sample carries no press or edge timestamp.
     */
    void handleTapButton(const hal::ButtonSample& button, std::uint32_t nowMs, std::uint32_t nowUs);

    /**
     * @brief Handles stop/reset and generic back navigation.
     * @param button Debounced STOP/BACK button sample.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void handleResetButton(const hal::ButtonSample& button, std::uint32_t nowMs);

    /**
     * @brief Executes the context-sensitive action for a short encoder press.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void handleShortEncoderPress(std::uint32_t nowMs);

    /**
     * @brief Short encoder press on the performance, channel quick select, mode select and
     *        mode-change screens.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void pressOnChannelScreen(std::uint32_t nowMs);

    /**
     * @brief Short encoder press on the template and preset slot screens.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void pressOnPresetScreen(std::uint32_t nowMs);

    /**
     * @brief Short encoder press on the preset overwrite, clear, factory-reset and name-entry
     *        dialogs.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void pressOnPresetDialog(std::uint32_t nowMs);

    /**
     * @brief Short encoder press on the Custom Groove slot list.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void pressOnGrooveSlots(std::uint32_t nowMs);

    /**
     * @brief Short encoder press on the Custom Groove confirmations and name entry.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void pressOnGrooveDialog(std::uint32_t nowMs);

    /**
     * @brief Short encoder press on the information popover, settings and sequencer editors.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void pressOnEditorScreen(std::uint32_t nowMs);

    /**
     * @brief Opens the selected channel/global menu after an encoder long press.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void handleLongEncoderPress(std::uint32_t nowMs);

    /**
     * @brief Toggles between PLAY and PAUSE without resetting phase.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence requests.
     */
    void toggleTransport(std::uint32_t nowMs);

    /**
     * @brief Stops transport and resets the master/global channel phase.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence requests.
     */
    void stopTransport(std::uint32_t nowMs);

    /**
     * @brief Feeds one tap into the rolling tempo estimator and visual tap sequence.
     * @param nowMs Current monotonic time in milliseconds, for display and persistence timing.
     * @param tapUs Tap time in the wrapping microsecond time base shared by panel and input taps.
     */
    void registerTapTempo(std::uint32_t nowMs, std::uint32_t tapUs);

    /**
     * @brief Ends a Tap Tempo sequence once its interval limit has passed, in microseconds.
     * @param nowUs Current time in the wrapping microsecond time base of the tap timestamps.
     */
    void serviceTapTempoSequence(std::uint32_t nowUs);

    /**
     * @brief Advances/clears the non-blocking Performance-screen Tap Tempo animation.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void serviceTapTempoFeedback(std::uint32_t nowMs);

    /**
     * @brief Returns true while an external clock owns the master tempo (decision D-02).
     * @return true for SOURCE EXTERNAL, or for SOURCE AUTO while the engine reports a lock.
     *
     * EXTERNAL always owns it; AUTO owns it while locked. Only tempo gestures are
     * blocked; menus and settings stay fully usable.
     */
    bool masterTempoIsExternal() const;

    /**
     * @brief Shows the transient EXTERNAL CLOCK notice instead of a tempo change.
     * @param nowMs Current monotonic time in milliseconds; starts the display time.
     */
    void showExternalTempoNotice(std::uint32_t nowMs);

    /**
     * @brief Hides the EXTERNAL CLOCK notice once its display time has elapsed.
     * @param nowMs Current monotonic time in milliseconds.
     *
     * The notice is also hidden as soon as the Performance screen is left.
     */
    void serviceExternalTempoNotice(std::uint32_t nowMs);

    /** @brief Installs the active pattern and step expression of every channel into the engine. */
    void reloadSequencerBanksIntoEngine();

    /**
     * @brief Hides the SAVE ERROR / UPDATE FIRMWARE notice once its display time has elapsed.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void servicePersistenceNotice(std::uint32_t nowMs);

    /**
     * @brief Opens a text settings page at a specific row and resets edit/scroll state.
     * @param page Settings page to open.
     * @param initialCursor Row to select; clamped to the last row of the page.
     */
    void openSettingsPage(SettingsPage page, std::uint8_t initialCursor = 0U);

    /**
     * @brief Opens the graphical horizontal mode selector and remembers its return screen.
     * @param returnScreen Screen that BACK returns to from the mode selector.
     */
    void openModeSelect(Screen returnScreen);

    /**
     * @brief Requests the highlighted mode function, asking for confirmation when it changes
     *        state.
     * @param nowMs Current monotonic time in milliseconds; not used by the implementation.
     */
    void commitSelectedMode(std::uint32_t nowMs);

    /**
     * @brief Applies the already confirmed mode function and routes to its most relevant screen.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence request.
     */
    void applyConfirmedMode(std::uint32_t nowMs);

    /**
     * @brief Routes the UI after a mode selection without adding another navigation layer.
     * @param modeFunction Mode function that was selected or applied.
     */
    void navigateAfterModeSelection(ModeFunction modeFunction);

    /**
     * @brief Returns whether the highlighted function already represents the active mode.
     * @param modeFunction Highlighted mode function.
     * @return true when it equals the mode of the selected channel or operating mode.
     */
    bool selectedModeIsCurrent(ModeFunction modeFunction) const;

    /** @brief Moves one level back through the settings hierarchy. */
    void backFromSettings();

    /**
     * @brief Activates the currently selected settings row or toggles edit mode.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void activateCurrentSetting(std::uint32_t nowMs);

    /** @brief Toggles value editing for a leaf settings row and schedules a redraw. */
    void toggleCurrentSettingEditing();

    /** @brief Activates one row on the settings root page. */
    void activateRootSetting();

    /** @brief Activates one row on the General settings page. */
    void activateGeneralSetting();

    /** @brief Activates one row on the Info settings page. */
    void activateInfoSetting();

    /** @brief Opens a full-text popover for the selected read-only informational row. */
    void openSelectedInformationPopover();

    /** @brief Activates one row on the Preferences settings page. */
    void activatePreferencesSetting();

    /** @brief Activates one row on an Independent-channel root page. */
    void activateChannelSetting();

    /** @brief Activates one row on the One Clock root page. */
    void activateUnifiedClockSetting();

    /** @brief Activates one row on a Timing page, including the Groove child page. */
    void activateTimingSetting();

    /** @brief Activates one row on the Groove settings page. */
    void activateGrooveSetting();

    /** @brief Activates one command or value on the Groove Editor menu. */
    void activateGrooveEditorMenuSetting();

    /** @brief Activates one command or value on the Groove Record menu. */
    void activateGrooveRecordMenuSetting();

    /** @brief Activates one row on the Divider Bank page. */
    void activateDividerBankSetting();

    /**
     * @brief Activates the Sequencer editor entry or one pattern command.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void activateSequencerPatternSetting(std::uint32_t nowMs);

    /**
     * @brief Returns the selected channel's active Sequencer 2.0 pattern when available.
     * @return Pointer into the pattern store, or nullptr without a store or with an invalid
     *         selected channel.
     */
    const SequencerPatternV2* activeSequencerPattern() const;

    /**
     * @brief Applies one encoder delta to PATTERN/LENGTH/ROTATE/DIRECTION/LOOP.
     * @param delta Signed detent count.
     * @param nowMs Current monotonic time in milliseconds; passed to the pattern store.
     */
    void adjustSequencerV2(std::int8_t delta, std::uint32_t nowMs);

    /**
     * @brief Toggles the selected 8-step viewport gate in the active persistent pattern.
     * @param nowMs Current monotonic time in milliseconds; passed to the pattern store.
     */
    void toggleSequencerStepV2(std::uint32_t nowMs);

    /**
     * @brief Executes one long-push Pattern operation (invert/clear/fill/copy/paste).
     * @param rowIndex Command row: 0 invert, 1 clear, 2 fill every second step, 3 copy,
     *        4 paste; other values are rejected.
     * @param nowMs Current monotonic time in milliseconds; passed to the stores.
     * @return true when the command took effect; false without a pattern store, for an
     *         unknown row, a paste without clipboard, or a failed store update.
     */
    bool executeSequencerPatternCommandV2(std::uint8_t rowIndex, std::uint32_t nowMs);

    /**
     * @brief Pushes the selected channel's active pattern snapshot into the real-time engine.
     * @param rescheduleChannel Forwarded to the engine: reschedule the channel immediately.
     */
    void synchronizeActiveSequencerPattern(bool rescheduleChannel);

    /**
     * @brief Pushes the selected channel's active sparse step-expression snapshot into the
     *        engine.
     */
    void synchronizeActiveSequencerStepMetadata();

    /** @brief Opens the detailed editor for the currently selected Sequencer step. */
    void openSequencerStepEditor();

    /**
     * @brief Returns the currently selected step's decoded expression override.
     * @return Stored metadata, or a default-constructed value without the stores or with an
     *         invalid selected channel.
     */
    SequencerStepMetadata activeSequencerStepMetadata() const;

    /**
     * @brief Applies one encoder delta to the selected step-expression field.
     * @param delta Signed detent count.
     * @param nowMs Current monotonic time in milliseconds; passed to the step store.
     */
    void adjustSequencerStepMetadata(std::int8_t delta, std::uint32_t nowMs);

    /**
     * @brief Toggles the selected detailed step's gate state.
     * @param nowMs Current monotonic time in milliseconds; passed to the pattern store.
     */
    void toggleSequencerStepGateFromDetail(std::uint32_t nowMs);

    /** @brief Keeps the selected settings row visible inside the five-row viewport. */
    void normalizeScrollOffset();

    /**
     * @brief Applies the confirmed factory template and returns to performance view.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence request.
     */
    void applySelectedTemplate(std::uint32_t nowMs);

    /** @brief Leaves APPLY TEMPLATE? without a change, back to the highlighted template. */
    void returnToTemplates();

    /**
     * @brief Executes or cancels the guarded Top-100 clear action.
     * @param nowMs Current monotonic time in milliseconds; transport is stopped before the
     *        leaderboard commit.
     */
    void confirmHighScoreClear(std::uint32_t nowMs);

    /**
     * @brief Executes or cancels the guarded full factory reset action.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void confirmFactoryReset(std::uint32_t nowMs);

    /**
     * @brief Opens the named-preset slot list for an explicit load or save operation.
     * @param action Operation the slot list performs on a press.
     */
    void openPresetSlots(PresetSlotAction action);

    /** @brief Initializes the 16-character high-score-style preset name editor. */
    void preparePresetName();

    /**
     * @brief Rotates the active preset-name character through the supported alphabet.
     * @param delta Signed detent count; wraps around the alphabet.
     */
    void adjustPresetNameCharacter(std::int8_t delta);

    /**
     * @brief Saves the edited name and complete current state into the selected preset slot.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence request.
     */
    void saveNamedPreset(std::uint32_t nowMs);

    /**
     * @brief Loads the selected preset without causing an implicit transport transition.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence request.
     */
    void loadSelectedPreset(std::uint32_t nowMs);

    /** @brief Opens the graphical Custom Groove editor with a runtime-only live preview. */
    void openGrooveEditor();

    /** @brief Opens the live TAP-driven Custom Groove recorder on the same draft format. */
    void openGrooveRecorder();

    /**
     * @brief Starts or stops the Groove recorder without changing the draft on stop.
     * @param nowMs Current monotonic time in milliseconds; used when starting from STOP
     *        switches the transport to PLAY.
     */
    void toggleGrooveRecording(std::uint32_t nowMs);

    /**
     * @brief Advances Pre-Count/playhead state from the live ClockEngine position.
     * @param snapshot Engine snapshot of this render cycle.
     */
    void serviceGrooveRecorder(const engine::EngineSnapshot& snapshot);

    /**
     * @brief Captures one high-resolution front-panel TAP into the nearest Groove step.
     * @param button TAP sample; its edge timestamp is used when valid.
     * @param nowUs Current time in the wrapping TIM5 microsecond time base.
     */
    void captureGrooveTap(const hal::ButtonSample& button, std::uint32_t nowUs);

    /** @brief Clears all recorded offsets while keeping current length/recorder settings. */
    void clearGrooveRecording();

    /** @brief Applies the draft to the runtime-only preview channels without persisting it. */
    void updateGroovePreview();

    /** @brief Clears all runtime preview overrides used by the Custom Groove editor. */
    void clearGroovePreview();

    /**
     * @brief Moves the selected Custom Groove marker by one fine/coarse offset increment.
     * @param delta Signed detent count.
     * @param coarse true scales each detent by 4 offset units, false by 1.
     */
    void adjustGrooveOffset(std::int8_t delta, bool coarse);

    /**
     * @brief Changes the editor zoom in four-step windows; direction >0 zooms in.
     * @param direction Sign selects the direction; 0 does nothing.
     */
    void adjustGrooveZoom(std::int8_t direction);

    /**
     * @brief Opens the fixed-record Custom Groove slot list.
     * @param action Operation the slot list performs on a press.
     */
    void openGrooveSlots(GrooveSlotAction action);

    /** @brief Loads the selected Custom Groove slot into the draft and live preview. */
    void loadSelectedGroove();

    /**
     * @brief Activates one stored Custom Groove directly from the normal Groove menu.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence request.
     */
    void activateSelectedGroove(std::uint32_t nowMs);

    /**
     * @brief Prepares the 16-character Custom Groove name editor with a generated default.
     * @param nowMs Current monotonic time in milliseconds; mixed into the generated name.
     */
    void prepareGrooveName(std::uint32_t nowMs);

    /**
     * @brief Loads the selected Custom Groove's current name into the rename editor.
     * @return false without a Custom Groove store or when the selected slot is empty.
     */
    bool prepareExistingGrooveName();

    /**
     * @brief Rotates the active Custom Groove name character.
     * @param delta Signed detent count; wraps around the alphabet.
     */
    void adjustGrooveNameCharacter(std::int8_t delta);

    /**
     * @brief Saves the current draft into the selected fixed-record Custom Groove slot.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence request.
     * @param keepExistingName true keeps the stored slot name instead of the edited one.
     */
    void saveCustomGroove(std::uint32_t nowMs, bool keepExistingName);

    /** @brief Renames the selected persistent Custom Groove without changing its pattern. */
    void renameSelectedGroove();

    /**
     * @brief Deletes the selected persistent Custom Groove after explicit confirmation.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence request.
     */
    void removeSelectedGroove(std::uint32_t nowMs);

    /**
     * @brief Commits the selected slot as the active Groove and returns to the Groove page.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence request.
     */
    void activateSavedCustomGroove(std::uint32_t nowMs);

    /**
     * @brief Returns from the editor, restoring the pre-editor state on discard.
     * @param discardChanges true restores the Groove settings saved when the editor opened.
     */
    void leaveGrooveEditor(bool discardChanges);

    /**
     * @brief Queues the complete current configuration for wear-coalesced persistence.
     * @param nowMs Current monotonic time in milliseconds; passed to the persistence service.
     */
    void persistCurrentState(std::uint32_t nowMs);

    /**
     * @brief Records front-panel activity and immediately restores a sleeping/dimmed OLED.
     * @param nowMs Current monotonic time in milliseconds; restarts the inactivity timer.
     */
    void noteUserActivity(std::uint32_t nowMs);

    /**
     * @brief Services STOP-mode screensaver, dimming, and OLED power-off policy.
     * @param nowMs Current monotonic time in milliseconds.
     * @return true when normal UI rendering stays suppressed for this cycle.
     */
    bool serviceStopModeDisplay(std::uint32_t nowMs);

    /**
     * @brief Clamps a signed integer to an inclusive range.
     * @param value Value to clamp.
     * @param minimum Inclusive lower bound.
     * @param maximum Inclusive upper bound; expected to be at least @p minimum.
     * @return @p value limited to [minimum, maximum].
     */
    static int clampInt(int value, int minimum, int maximum);

    /**
     * @brief Moves a cyclic list index by a signed encoder delta.
     * @param index Current index; values outside the list are wrapped as well.
     * @param delta Signed detent count; any magnitude, including the full int8 range.
     * @param count Number of list entries; zero yields index 0.
     * @return Index in [0, count), never negative.
     */
    static std::uint8_t wrapIndex(std::uint8_t index, int delta, std::uint8_t count);

    /** Application state edited by the UI; owned by the caller. */
    ClockState& state_;
    /** Real-time engine that receives committed configuration changes. */
    engine::ClockEngine& engine_;
    /** Screen renderer and display power/dimming interface. */
    UiRenderer& renderer_;
    /** Persistence service for transport, current state and named presets. */
    services::PersistentStateService& persistentState_;
    /** Custom Groove library; nullptr disables the Groove slot functions. */
    services::CustomGrooveStore* customGrooveStore_ = nullptr;
    /** Sequencer 2.0 pattern store; nullptr selects the legacy step-toggle path. */
    services::SequencerPatternStore* sequencerPatternStore_ = nullptr;
    /** Sequencer 2.0 per-step expression store; nullptr disables the step editor. */
    services::SequencerStepStore* sequencerStepStore_ = nullptr;
    /** Optional arcade leaderboard; nullptr hides the HI-SCORES reset. */
    game::ArcadeLeaderboardStore* leaderboard_ = nullptr;
    /** Optional source of the SYNC/RESET levels for the input diagnostics page. */
    const hal::ExternalInputCapture* externalInputs_ = nullptr;
    /** Optional source of the gate output levels for the output diagnostics page. */
    const hal::GateOutputDriver* gateOutputs_ = nullptr;
    /** Validated value mutation for settings rows. */
    SettingsEditor settingsEditor_;
    /** Rolling Tap Tempo estimator. */
    services::TapTempo tapTempo_{};
    /** Live TAP-driven Custom Groove recorder. */
    services::GrooveRecorder grooveRecorder_{};
    /** Current screen, cursor and edit state; read by the renderer. */
    NavigationState navigation_{};

    /** A Tap Tempo sequence is in progress; cleared when its interval limit passes. */
    bool tapVisualSequenceActive_ = false;
    /** Timestamp of the last registered tap, in the wrapping microsecond time base. */
    std::uint32_t lastTapAtUs_ = 0U;
    /** Start of the current tap indicator animation, in milliseconds. */
    std::uint32_t tapIndicatorStartedAtMs_ = 0U;
    /** Time the EXTERNAL CLOCK notice was last shown, in milliseconds. */
    std::uint32_t externalTempoNoticeShownAtMs_ = 0U;
    /** Time the persistence notice was shown, in milliseconds. */
    std::uint32_t persistenceNoticeShownAtMs_ = 0U;

    /** A redraw is pending. */
    bool renderDirty_ = true;
    /** Time of the last rendered frame, in milliseconds; limits the refresh rate. */
    std::uint32_t lastRenderAtMs_ = 0U;
    /** Selected channel's engine step at the last render check; 0xFF before the first. */
    std::uint8_t lastRenderedChannelStep_ = 0xFFU;
    /** External lock state at the last render check. */
    bool lastRenderedExternalLocked_ = false;
    /** Pre-Count activity at the last render check. */
    bool lastRenderedPreCountActive_ = false;
    /** Remaining Pre-Count count at the last render check. */
    std::uint8_t lastRenderedPreCountRemaining_ = 0U;
    /** The lastRendered engine fields hold a valid comparison value. */
    bool hasRenderedEngineStatus_ = false;
    /** Diagnostics levels at the last render check of a diagnostics page. */
    DiagnosticSnapshot lastDiagnosticSnapshot_{};
    /** lastDiagnosticSnapshot_ is valid; cleared whenever no diagnostics page is shown. */
    bool hasRenderedDiagnosticSnapshot_ = false;

    /** TAP + encoder push opened the settings; set until both are released. */
    bool settingsChordActive_ = false;
    /** TAP is held in the current control sample. */
    bool tapModifierPressed_ = false;
    /** An encoder press is being timed for the long-press decision. */
    bool encoderPressTracking_ = false;
    /** The current encoder press already triggered its long-press action. */
    bool encoderLongPressHandled_ = false;
    /** Start of the current encoder press, in milliseconds. */
    std::uint32_t encoderPressedAtMs_ = 0U;
    /** TAP-Turn opened the mode selector; the TAP release commits the selected mode. */
    bool modeTapTurnActive_ = false;
    /** TAP was held as a modifier while the encoder turned; its release is no TAP action. */
    bool tapPressConsumedByTurn_ = false;
    /** A press that woke the OLED is hidden until it is released (D-08). */
    bool wakeSwallowsEncoderButton_ = false;
    /** A TAP press that woke the OLED is hidden until it is released (D-08). */
    bool wakeSwallowsTapButton_ = false;
    /** A STOP/BACK press that woke the OLED outside Performance is hidden until released. */
    bool wakeSwallowsResetButton_ = false;
    /** lastUserActivityAtMs_ holds a valid time. */
    bool activityClockInitialized_ = false;
    /** The STOP-mode screensaver is showing. */
    bool screensaverActive_ = false;
    /** The OLED is dimmed by the STOP-mode policy. */
    bool displayDimmed_ = false;
    /** The OLED is switched off by the STOP-mode policy. */
    bool displayPoweredOff_ = false;
    /** Time of the last user activity, in milliseconds; base of the STOP-mode timeouts. */
    std::uint32_t lastUserActivityAtMs_ = 0U;
    /** Wrapping count of control samples with user activity. */
    std::uint32_t controlActivityRevision_ = 0U;
    /** Time of the last screensaver frame, in milliseconds. */
    std::uint32_t lastScreensaverFrameAtMs_ = 0U;
    /** Frame counter passed to the screensaver renderer; reset on user activity. */
    std::uint32_t screensaverFrameIndex_ = 0U;

    /** Groove settings of the edited target when the editor or recorder opened. */
    GrooveSettings grooveEditorOriginalSettings_{};
    /** The Groove draft has unsaved changes; BACK then asks for a discard confirmation. */
    bool grooveEditorDirty_ = false;
    /** A confirmed discard continues with the slot list for loading into the editor. */
    bool grooveLoadPendingAfterDiscard_ = false;
    /** PLAY was held for a Groove zoom; its release is no transport action (F-039). */
    bool transportPressConsumedByGrooveZoom_ = false;
    /** Groove workspace (editor or recorder) that dialogs and slot lists return to. */
    Screen grooveWorkspaceScreen_ = Screen::GrooveEditor;
    /** Pattern copied by the Sequencer COPY command. */
    SequencerPatternV2 sequencerPatternClipboard_{};
    /** Step expression words copied together with the pattern. */
    std::array<SequencerStepMetadataWord, kSequencerMaximumSteps> sequencerStepClipboard_{};
    /** sequencerPatternClipboard_ holds a copied pattern. */
    bool sequencerPatternClipboardValid_ = false;
    /** sequencerStepClipboard_ holds copied step expression. */
    bool sequencerStepClipboardValid_ = false;
    /** TAP + push opened the step editor; the following TAP release is ignored. */
    bool sequencerTapPressConsumed_ = false;
    /** Seed of the default-name generator for this boot session. */
    std::uint32_t generatedNameSeed_ = 0xC10C2026U;
    /** Number of default names generated since the last seeding. */
    std::uint32_t generatedNameSequence_ = 0U;
};

}  // namespace clockfw::ui
