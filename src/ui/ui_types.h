/**
 * @file ui_types.h
 * @brief User-interface navigation state and screen identifiers.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstdint>

#include "domain/custom_groove.h"

namespace clockfw::ui {

/**
 * Rows a list shows at once. The controller's scroll window and the renderer use the same
 * constant, so the cursor is always on a drawn row (F-019).
 */
/** Factory-template list: four 12-pixel rows below the title. */
inline constexpr std::uint8_t kVisibleTemplateRows = 4U;
/** Named-preset and Custom Groove slot lists: five 10-pixel rows below the title. */
inline constexpr std::uint8_t kVisibleSlotRows = 5U;
/** Text settings pages: five rows below the title. */
inline constexpr std::uint8_t kVisibleSettingsRows = 5U;

/** @brief Top-level OLED working screens. */
enum class Screen : std::uint8_t {
    Performance,
    ChannelQuickSelect,
    ModeSelect,
    ModeChangeConfirm,
    Settings,
    SequencerEditor,
    SequencerStepEditor,
    Templates,
    PresetSlots,
    OverwriteConfirm,
    HighScoreClearConfirm,
    FactoryResetConfirm,
    NameEntry,
    GrooveEditor,
    GrooveRecorder,
    GrooveSlots,
    GrooveOverwriteConfirm,
    GrooveDeleteConfirm,
    GrooveDiscardConfirm,
    GrooveNameEntry,
    InformationPopover,
    /**
     * APPLY TEMPLATE? with NO preselected before a factory template replaces the configuration
     * (D-09).
     */
    TemplateApplyConfirm
};

/** @brief Hierarchical text settings pages. */
enum class SettingsPage : std::uint8_t {
    Root,
    General,
    InputAssignments,
    Hardware,
    Diagnostics,
    DiagnosticsInputs,
    DiagnosticsOutputs,
    Master,
    Sync,
    Preferences,
    Screensaver,
    Info,
    Support,
    Licenses,
    Updates,
    Channel,
    ChannelTiming,
    ChannelOutput,
    Clock,
    Euclid,
    Sequencer,
    SequencerPattern,
    UnifiedClock,
    UnifiedTiming,
    UnifiedOutput,
    Groove,
    GrooveEditorMenu,
    GrooveRecordMenu,
    DividerBank
};

/** @brief One action exposed by the horizontally scrolling mode selector. */
enum class ModeFunction : std::uint8_t { Off, Clock, Euclid, Sequencer, UnifiedClock, DividerBank };

/** @brief Operation being performed on the explicit user preset slots. */
enum class PresetSlotAction : std::uint8_t { Load, Save };

/** @brief Persistence notice shown on the Performance screen (F-021, F-045). */
enum class PersistenceNotice : std::uint8_t { None, SaveError, SaveLocked };

/** @brief Operation being performed on Custom Groove slots. */
enum class GrooveSlotAction : std::uint8_t { LoadEditor, Activate, Save, Rename, Delete };

/** @brief Loop policy used by the live Custom Groove recorder. */
enum class GrooveRecordMode : std::uint8_t { OneShot, Endless };

/** @brief Front-panel recording state shown by the graphical Groove recorder. */
enum class GrooveRecordState : std::uint8_t { Ready, PreCount, Recording };

/** @brief Live digital levels shown by the hardware diagnostics pages. */
struct DiagnosticSnapshot {
    /** SYNC input level is high. */
    bool syncHigh = false;
    /** RESET input level is high. */
    bool resetHigh = false;
    /** Gate output levels, one entry per channel; true is high. */
    std::array<bool, 8U> outputs{};

    /**
     * @brief Compares all levels.
     * @param other Snapshot to compare with.
     * @return true when every level matches.
     */
    bool operator==(const DiagnosticSnapshot& other) const {
        return syncHigh == other.syncHigh && resetHigh == other.resetHigh &&
            outputs == other.outputs;
    }
    /**
     * @brief Negation of operator==.
     * @param other Snapshot to compare with.
     * @return true when any level differs.
     */
    bool operator!=(const DiagnosticSnapshot& other) const {
        return !(*this == other);
    }
};

/** @brief Complete mutable UI navigation state, deliberately separate from musical state. */
struct NavigationState {
    /** Screen that is shown and receives the controls. */
    Screen screen = Screen::Performance;
    /** Settings page shown while screen is Screen::Settings. */
    SettingsPage settingsPage = SettingsPage::Root;
    /** Zero-based channel that channel menus and editors work on. */
    std::uint8_t selectedChannel = 0U;
    /** Selected row or item on the current screen. */
    std::uint8_t cursor = 0U;
    /** Screen that BACK returns to from the mode selector. */
    Screen modeSelectReturnScreen = Screen::Settings;
    /** Mode function waiting for confirmation on the mode-change dialog. */
    ModeFunction pendingModeFunction = ModeFunction::Clock;
    /** Screen that BACK returns to from the settings root page. */
    Screen settingsExitScreen = Screen::Performance;
    /** First visible row of a scrolling list. */
    std::uint8_t scrollOffset = 0U;
    /** The selected row is in value-edit mode. */
    bool editing = false;
    /** Selected step within the 8-step Sequencer editor page. */
    std::uint8_t sequencerCursor = 0U;
    /** Shown 8-step page of the Sequencer editor; the absolute step is page * 8 + cursor. */
    std::uint8_t sequencerPage = 0U;
    /** 0 hides the Performance-screen tap indicator; 1..4 select shrinking animation frames. */
    std::uint8_t tapIndicatorFrame = 0U;
    /** True while the "tempo is set externally" notice is shown on the Performance screen. */
    bool externalTempoNotice = false;
    /** Persistence failure notice; takes precedence over the EXTERNAL CLOCK notice. */
    PersistenceNotice persistenceNotice = PersistenceNotice::None;
    /** True when the selected ranked Easter egg has created a durable leaderboard record. */
    bool highScoreResetAvailable = false;
    /** Template that APPLY TEMPLATE? applies on YES. */
    std::uint8_t pendingTemplate = 0U;
    /** Operation of the named-preset slot list. */
    PresetSlotAction presetSlotAction = PresetSlotAction::Load;
    /** Named-preset slot that load, save and name entry work on. */
    std::uint8_t selectedPresetSlot = 0U;
    /** Edited character position in the preset or Custom Groove name entry. */
    std::uint8_t nameCharacterIndex = 0U;
    /** Edited preset name: 16 characters and a terminating NUL. */
    std::array<char, 17U> presetNameBuffer{};
    /** Operation of the Custom Groove slot list. */
    GrooveSlotAction grooveSlotAction = GrooveSlotAction::LoadEditor;
    /** Custom Groove slot that load, save, rename and delete work on. */
    std::uint8_t selectedGrooveSlot = 0U;
    /** Selected step of the Groove draft; follows the playhead while recording. */
    std::uint8_t grooveCursor = 0U;
    /** 0 = FIT; otherwise visible window size in four-step increments. */
    std::uint8_t grooveZoomSteps = 0U;
    /** Custom Groove being edited or recorded; previewed live but not persisted until saved. */
    CustomGroovePattern grooveDraft{};
    /** Loop policy of the Groove recorder. */
    GrooveRecordMode grooveRecordMode = GrooveRecordMode::OneShot;
    /** Recorder state copied from the Groove recorder for display. */
    GrooveRecordState grooveRecordState = GrooveRecordState::Ready;
    /** Configured Pre-Count length in beats; the record menu limits it to 0..64. */
    std::uint8_t grooveRecordCountInBeats = 4U;
    /** Remaining Pre-Count beats of the running recorder. */
    std::uint8_t grooveRecordCountInRemaining = 0U;
    /** Recorder playhead step within the Groove draft. */
    std::uint8_t grooveRecordPlayheadStep = 0U;
    /** Recorder playhead position inside its step, in 1/256 step. */
    std::uint8_t grooveRecordPlayheadPhase256 = 0U;
    /** One bit per draft step that holds a captured tap. */
    std::uint64_t grooveRecordCapturedMask = 0U;
    /** Edited Custom Groove name: 16 characters and a terminating NUL. */
    std::array<char, 17U> grooveNameBuffer{};
    /** Title/value copied from one read-only settings row while its detail popover is open. */
    std::array<char, 18U> informationPopoverTitle{};
    /** Value text of the read-only row shown in the information popover. */
    std::array<char, 64U> informationPopoverValue{};
};

}  // namespace clockfw::ui
