/**
 * @file ui_controller_encoder_delta.cpp
 * @brief Encoder-turn routing including Custom Groove editing gestures.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "ui/ui_controller_encoder_delta.h"

#include "services/template_service.h"
#include "ui/menu_model.h"
#include "ui/mode_functions.h"

namespace clockfw::ui {

void UiController::handleEncoderDelta(
    const std::int8_t delta,
    const bool tapPressed,
    const bool transportPressed,
    const std::uint32_t nowMs) {
    // Screen groups in the original order; the first group that owns the screen handles it.
    if (handleGrooveScreenDelta(delta, tapPressed, transportPressed)) {
        return;
    }
    if (handleChannelScreenDelta(delta, tapPressed, nowMs)) {
        return;
    }
    if (handlePresetScreenDelta(delta)) {
        return;
    }
    if (handleSettingsScreenDelta(delta, nowMs)) {
        return;
    }
    (void)handleSequencerScreenDelta(delta, nowMs);  // no other screen reacts to a detent
}

bool UiController::handleGrooveScreenDelta(
    const std::int8_t delta,
    const bool tapPressed,
    const bool transportPressed) {
    if (navigation_.screen == Screen::GrooveEditor) {
        if (transportPressed) {
            transportPressConsumedByGrooveZoom_ = true;
            adjustGrooveZoom(delta);
        } else if (tapPressed) {
            adjustGrooveOffset(delta, true);
        } else {
            adjustGrooveOffset(delta, false);
        }
        return true;
    }

    if (navigation_.screen == Screen::GrooveRecorder) {
        if (transportPressed) {
            transportPressConsumedByGrooveZoom_ = true;
            adjustGrooveZoom(delta);
        }
        return true;
    }

    if (navigation_.screen == Screen::GrooveSlots) {
        navigation_.cursor = static_cast<std::uint8_t>(clampInt(
            static_cast<int>(navigation_.cursor) + delta,
            0,
            static_cast<int>(kCustomGrooveSlotCount - 1U)));
        if (navigation_.cursor < navigation_.scrollOffset)
            navigation_.scrollOffset = navigation_.cursor;
        if (navigation_.cursor >= navigation_.scrollOffset + kVisibleSlotRows) {
            navigation_.scrollOffset =
                static_cast<std::uint8_t>(navigation_.cursor - (kVisibleSlotRows - 1U));
        }
        invalidate();
        return true;
    }

    if (navigation_.screen == Screen::GrooveNameEntry) {
        adjustGrooveNameCharacter(delta);
        invalidate();
        return true;
    }

    if (navigation_.screen == Screen::GrooveOverwriteConfirm ||
        navigation_.screen == Screen::GrooveDeleteConfirm ||
        navigation_.screen == Screen::GrooveDiscardConfirm) {
        navigation_.cursor =
            static_cast<std::uint8_t>(clampInt(static_cast<int>(navigation_.cursor) + delta, 0, 1));
        invalidate();
        return true;
    }
    return false;
}

bool UiController::handleChannelScreenDelta(
    const std::int8_t delta,
    const bool tapPressed,
    const std::uint32_t nowMs) {
    if (navigation_.screen == Screen::ChannelQuickSelect && tapPressed) {
        if (!modeTapTurnActive_) {
            if (state_.operatingMode == OperatingMode::Independent &&
                navigation_.cursor < kChannelCount) {
                navigation_.selectedChannel = navigation_.cursor;
            }
            openModeSelect(Screen::ChannelQuickSelect);
            modeTapTurnActive_ = true;
        }
        navigation_.cursor =
            wrapIndex(navigation_.cursor, delta, static_cast<std::uint8_t>(kModeFunctions.size()));
        invalidate();
        return true;
    }

    if (navigation_.screen == Screen::ChannelQuickSelect) {
        if (state_.operatingMode == OperatingMode::Independent) {
            navigation_.cursor =
                wrapIndex(navigation_.cursor, delta, static_cast<std::uint8_t>(kChannelCount));
            invalidate();
        }
        return true;
    }

    if (navigation_.screen == Screen::Performance) {
        if (masterTempoIsExternal()) {
            showExternalTempoNotice(nowMs);
            return true;
        }
        settingsEditor_.changeMasterTempo(delta);
        invalidate();
        return true;
    }

    if (navigation_.screen == Screen::ModeSelect) {
        navigation_.cursor =
            wrapIndex(navigation_.cursor, delta, static_cast<std::uint8_t>(kModeFunctions.size()));
        invalidate();
        return true;
    }
    return false;
}

bool UiController::handlePresetScreenDelta(const std::int8_t delta) {
    if (navigation_.screen == Screen::Templates) {
        navigation_.cursor = static_cast<std::uint8_t>(clampInt(
            static_cast<int>(navigation_.cursor) + delta,
            0,
            static_cast<int>(services::TemplateService::kTemplateCount - 1U)));
        if (navigation_.cursor < navigation_.scrollOffset) {
            navigation_.scrollOffset = navigation_.cursor;
        }
        if (navigation_.cursor >= navigation_.scrollOffset + kVisibleTemplateRows) {
            navigation_.scrollOffset =
                static_cast<std::uint8_t>(navigation_.cursor - (kVisibleTemplateRows - 1U));
        }
        invalidate();
        return true;
    }

    if (navigation_.screen == Screen::PresetSlots) {
        navigation_.cursor = static_cast<std::uint8_t>(clampInt(
            static_cast<int>(navigation_.cursor) + delta,
            0,
            static_cast<int>(services::PersistentStateService::kUserPresetSlotCount - 1U)));
        if (navigation_.cursor < navigation_.scrollOffset) {
            navigation_.scrollOffset = navigation_.cursor;
        }
        if (navigation_.cursor >= navigation_.scrollOffset + kVisibleSlotRows) {
            navigation_.scrollOffset =
                static_cast<std::uint8_t>(navigation_.cursor - (kVisibleSlotRows - 1U));
        }
        invalidate();
        return true;
    }

    if (navigation_.screen == Screen::NameEntry) {
        adjustPresetNameCharacter(delta);
        invalidate();
        return true;
    }

    if (navigation_.screen == Screen::OverwriteConfirm) {
        navigation_.cursor =
            static_cast<std::uint8_t>(clampInt(static_cast<int>(navigation_.cursor) + delta, 0, 1));
        invalidate();
        return true;
    }

    if (navigation_.screen == Screen::ModeChangeConfirm ||
        navigation_.screen == Screen::HighScoreClearConfirm ||
        navigation_.screen == Screen::FactoryResetConfirm ||
        navigation_.screen == Screen::TemplateApplyConfirm) {
        navigation_.cursor =
            static_cast<std::uint8_t>(clampInt(static_cast<int>(navigation_.cursor) + delta, 0, 1));
        invalidate();
        return true;
    }
    return false;
}

bool UiController::handleSettingsScreenDelta(const std::int8_t delta, const std::uint32_t nowMs) {
    if (navigation_.screen == Screen::Settings) {
        if (navigation_.editing && navigation_.settingsPage == SettingsPage::GrooveEditorMenu) {
            adjustGrooveEditorMenuValue(delta);
        } else if (navigation_.editing &&
                   navigation_.settingsPage == SettingsPage::GrooveRecordMenu) {
            adjustGrooveRecordMenuValue(delta);
        } else if (navigation_.editing && navigation_.settingsPage == SettingsPage::Sequencer &&
                   sequencerPatternStore_ != nullptr) {
            adjustSequencerV2(delta, nowMs);
        } else if (navigation_.editing) {
            settingsEditor_.adjust(
                navigation_.settingsPage, navigation_.cursor, navigation_.selectedChannel, delta);
            invalidate();
        } else {
            navigation_.cursor = static_cast<std::uint8_t>(clampInt(
                static_cast<int>(navigation_.cursor) + delta,
                0,
                static_cast<int>(settingsPageItemCount(
                    navigation_.settingsPage,
                    state_.channels[navigation_.selectedChannel].common.mode,
                    navigation_.highScoreResetAvailable)) -
                    1));
            normalizeScrollOffset();
            invalidate();
        }
        return true;
    }
    return false;
}

void UiController::adjustGrooveEditorMenuValue(const std::int8_t delta) {
    if (navigation_.cursor == 2U) {
        adjustGrooveZoom(delta);
    } else if (navigation_.cursor == 3U) {
        const int nextLength = clampInt(
            static_cast<int>(navigation_.grooveDraft.length) + delta,
            1,
            static_cast<int>(kCustomGrooveMaximumSteps));
        navigation_.grooveDraft.length = static_cast<std::uint8_t>(nextLength);
        if (navigation_.grooveCursor >= navigation_.grooveDraft.length) {
            navigation_.grooveCursor =
                static_cast<std::uint8_t>(navigation_.grooveDraft.length - 1U);
        }
        grooveEditorDirty_ = true;
        updateGroovePreview();
        invalidate();
    }
}

void UiController::adjustGrooveRecordMenuValue(const std::int8_t delta) {
    if (navigation_.cursor == 0U) {
        navigation_.grooveRecordMode = navigation_.grooveRecordMode == GrooveRecordMode::OneShot
            ? GrooveRecordMode::Endless
            : GrooveRecordMode::OneShot;
        grooveRecorder_.setMode(
            navigation_.grooveRecordMode == GrooveRecordMode::OneShot
                ? services::GrooveRecordMode::OneShot
                : services::GrooveRecordMode::Endless);
        invalidate();
    } else if (navigation_.cursor == 1U) {
        navigation_.grooveRecordCountInBeats = static_cast<std::uint8_t>(
            clampInt(static_cast<int>(navigation_.grooveRecordCountInBeats) + delta, 0, 64));
        grooveRecorder_.setCountInBeats(navigation_.grooveRecordCountInBeats);
        invalidate();
    } else if (navigation_.cursor == 2U) {
        const std::uint8_t previousLength = navigation_.grooveDraft.length;
        const int nextLength = clampInt(
            static_cast<int>(navigation_.grooveDraft.length) + delta,
            1,
            static_cast<int>(kCustomGrooveMaximumSteps));
        navigation_.grooveDraft.length = static_cast<std::uint8_t>(nextLength);
        if (navigation_.grooveDraft.length < previousLength) {
            for (std::size_t step = navigation_.grooveDraft.length;
                 step < navigation_.grooveDraft.offsets256.size();
                 ++step) {
                navigation_.grooveDraft.offsets256[step] = 0;
            }
        }
        navigation_.grooveRecordCapturedMask &= navigation_.grooveDraft.length >= 64U
            ? UINT64_MAX
            : ((1ULL << navigation_.grooveDraft.length) - 1ULL);
        // The recorder is stopped while its menu is open. Re-seed its
        // capture bookkeeping after a length edit so a later pass cannot
        // resurrect captured bits that are now outside the pattern.
        grooveRecorder_.reset(
            navigation_.grooveRecordMode == GrooveRecordMode::OneShot
                ? services::GrooveRecordMode::OneShot
                : services::GrooveRecordMode::Endless,
            navigation_.grooveRecordCountInBeats,
            navigation_.grooveRecordCapturedMask);
        grooveEditorDirty_ = true;
        updateGroovePreview();
        invalidate();
    } else if (navigation_.cursor == 3U) {
        adjustGrooveZoom(delta);
    }
}

bool UiController::handleSequencerScreenDelta(const std::int8_t delta, const std::uint32_t nowMs) {
    if (navigation_.screen == Screen::SequencerStepEditor) {
        if (navigation_.editing) {
            adjustSequencerStepMetadata(delta, nowMs);
        } else {
            navigation_.cursor = static_cast<std::uint8_t>(
                clampInt(static_cast<int>(navigation_.cursor) + delta, 0, 4));
            invalidate();
        }
        return true;
    }

    if (navigation_.screen == Screen::SequencerEditor) {
        const SequencerPatternV2* const pattern = activeSequencerPattern();
        const std::uint8_t length = pattern != nullptr
            ? pattern->length
            : state_.channels[navigation_.selectedChannel].sequencer.length;
        int absoluteStep = navigation_.sequencerPage * 8 + navigation_.sequencerCursor + delta;
        absoluteStep = clampInt(absoluteStep, 0, static_cast<int>(length) - 1);
        navigation_.sequencerPage = static_cast<std::uint8_t>(absoluteStep / 8);
        navigation_.sequencerCursor = static_cast<std::uint8_t>(absoluteStep % 8);
        invalidate();
        return true;
    }
    return false;
}

}  // namespace clockfw::ui
