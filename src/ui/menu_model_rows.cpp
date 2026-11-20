/**
 * @file menu_model_rows.cpp
 * @brief Localized label and value formatting of every settings-menu row.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "ui/menu_model.h"
#include "ui/menu_model_channel.h"
#include "ui/menu_model_general.h"

#include <cstdio>

#include "config.h"
#include "domain/clock_labels.h"
#include "domain/clock_options.h"
#include "text_format.h"
#include "ui/menu_model_groove.h"
#include "ui/menu_model_sync.h"
#include "ui/text_formatter.h"
#include "ui_text.h"
#include "version.h"

namespace clockfw::ui {
namespace {

/** Copies one localized static string into a bounded menu field. */
void copyText(char* const destination, const std::size_t size, const text::TextId id) {
    text::formatInto(destination, size, "%s", text::get(id));
}

/** Fills one row of the Root settings page. */
void fillRootRow(MenuRow& row, const std::uint8_t rowIndex, const bool highScoreResetAvailable) {
    if (rowIndex < 5U) {
        constexpr text::TextId kLabels[] = {
            text::TextId::GeneralSettings,
            text::TextId::ChannelSettings,
            text::TextId::Presets,
            text::TextId::SupportClock,
            text::TextId::Info};
        copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
        copyText(row.value, sizeof(row.value), text::TextId::Arrow);
    } else if (highScoreResetAvailable && rowIndex == kRootHighScoresRow) {
        copyText(row.label, sizeof(row.label), text::TextId::HighScores);
        copyText(row.value, sizeof(row.value), text::TextId::Clear);
    } else {
        copyText(row.label, sizeof(row.label), text::TextId::PhaseReset);
    }
}

/** Fills one row of the Support settings page. */
void fillSupportRow(MenuRow& row) {
    // Support goes through Ko-fi only; the page keeps its one entry.
    copyText(row.label, sizeof(row.label), text::TextId::Kofi);
    text::formatInto(row.value, sizeof(row.value), "%s", text::get(text::TextId::KofiUrl));
    row.expandableInformation = true;
}

/** Fills one row of the General settings page. */
void fillGeneralRow(MenuRow& row, const std::uint8_t rowIndex) {
    constexpr text::TextId kLabels[] = {
        text::TextId::ModeClockLong,
        text::TextId::Inputs,
        text::TextId::Screensaver,
        text::TextId::Diagnostics,
        text::TextId::Hardware};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
    copyText(row.value, sizeof(row.value), text::TextId::Arrow);
}

/** Fills one row of the InputAssignments settings page. */
void fillInputAssignmentsRow(MenuRow& row, const std::uint8_t rowIndex, const ClockState& state) {
    if (rowIndex == 0U) {
        copyText(row.label, sizeof(row.label), text::TextId::Input1);
        text::formatInto(
            row.value, sizeof(row.value), "%s", inputFunctionLabel(state.inputs.input1));
    } else if (rowIndex == 1U) {
        copyText(row.label, sizeof(row.label), text::TextId::Input2);
        text::formatInto(
            row.value, sizeof(row.value), "%s", inputFunctionLabel(state.inputs.input2));
    } else {
        copyText(row.label, sizeof(row.label), text::TextId::InputConfig);
        copyText(row.value, sizeof(row.value), text::TextId::Arrow);
    }
}

/** Fills one row of the Hardware settings page. */
void fillHardwareRow(MenuRow& row, const std::uint8_t rowIndex, const ClockState& state) {
    if (rowIndex == 0U) {
        copyText(row.label, sizeof(row.label), text::TextId::EncoderDirection);
        copyText(
            row.value,
            sizeof(row.value),
            state.device.encoderDirectionReversed ? text::TextId::Reversed : text::TextId::Normal);
    } else {
        copyText(row.label, sizeof(row.label), text::TextId::Orientation);
        copyText(
            row.value,
            sizeof(row.value),
            state.device.displayRotated180 ? text::TextId::Degrees180 : text::TextId::Degrees0);
    }
}

/** Fills one row of the Master settings page. */
void fillMasterRow(MenuRow& row, const std::uint8_t rowIndex, const ClockState& state) {
    if (rowIndex == 0U) {
        copyText(row.label, sizeof(row.label), text::TextId::Tempo);
        text::formatInto(row.value, sizeof(row.value), "%u", state.bpm);
    } else if (rowIndex == 1U) {
        copyText(row.label, sizeof(row.label), text::TextId::MinimumTempo);
        text::formatInto(row.value, sizeof(row.value), "%u", state.tempoRange.minimumBpm);
    } else if (rowIndex == 2U) {
        copyText(row.label, sizeof(row.label), text::TextId::MaximumTempo);
        text::formatInto(row.value, sizeof(row.value), "%u", state.tempoRange.maximumBpm);
    } else if (rowIndex == 3U) {
        copyText(row.label, sizeof(row.label), text::TextId::MeterBeats);
        text::formatInto(row.value, sizeof(row.value), "%u", state.masterMeter.beats);
    } else if (rowIndex == 4U) {
        copyText(row.label, sizeof(row.label), text::TextId::MeterUnit);
        text::formatInto(row.value, sizeof(row.value), "%u", state.masterMeter.unit);
    } else {
        copyText(row.label, sizeof(row.label), text::TextId::PreCount);
        if (state.preCountSteps == 0U) {
            copyText(row.value, sizeof(row.value), text::TextId::Off);
        } else {
            text::formatInto(row.value, sizeof(row.value), "%u", state.preCountSteps);
        }
    }
}

/** Fills one row of the Preferences settings page. */
void fillPreferencesRow(MenuRow& row, const std::uint8_t rowIndex) {
    constexpr text::TextId kLabels[] = {
        text::TextId::Current,
        text::TextId::LoadPreset,
        text::TextId::SavePreset,
        text::TextId::Templates};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
    if (rowIndex == 0U) {
        copyText(row.value, sizeof(row.value), text::TextId::AutoSave);
    } else {
        copyText(row.value, sizeof(row.value), text::TextId::Arrow);
    }
}

/** Fills one row of the GrooveEditorMenu settings page. */
void fillGrooveEditorMenuRow(MenuRow& row, const std::uint8_t rowIndex) {
    constexpr text::TextId kLabels[] = {
        text::TextId::Save, text::TextId::Load, text::TextId::Zoom, text::TextId::Length};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
    if (rowIndex < 2U) {
        copyText(row.value, sizeof(row.value), text::TextId::Arrow);
    }
}

/** Fills one row of the GrooveRecordMenu settings page. */
void fillGrooveRecordMenuRow(MenuRow& row, const std::uint8_t rowIndex) {
    constexpr text::TextId kLabels[] = {
        text::TextId::Mode,
        text::TextId::CountIn,
        text::TextId::Length,
        text::TextId::Zoom,
        text::TextId::Save,
        text::TextId::Editor,
        text::TextId::Clear};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
    if (rowIndex == 4U || rowIndex == 5U) {
        copyText(row.value, sizeof(row.value), text::TextId::Arrow);
    }
}

/** Fills one row of the Clock settings page. */
void fillClockRow(MenuRow& row, const std::uint8_t rowIndex, const ChannelConfig& channel) {
    copyText(
        row.label,
        sizeof(row.label),
        rowIndex == 0U ? text::TextId::MeterBeats : text::TextId::MeterUnit);
    text::formatInto(
        row.value,
        sizeof(row.value),
        "%u",
        rowIndex == 0U ? channel.clock.meter.beats : channel.clock.meter.unit);
}

/** Fills one row of the Euclid settings page. */
void fillEuclidRow(MenuRow& row, const std::uint8_t rowIndex, const ChannelConfig& channel) {
    constexpr text::TextId kLabels[] = {
        text::TextId::Steps, text::TextId::Hits, text::TextId::Rotate};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
    const std::uint8_t value = rowIndex == 0U
        ? channel.euclid.steps
        : (rowIndex == 1U ? channel.euclid.hits : channel.euclid.rotation);
    text::formatInto(row.value, sizeof(row.value), "%u", value);
}

/** Fills one row of the Sequencer settings page. */
void fillSequencerRow(MenuRow& row, const std::uint8_t rowIndex, const ChannelConfig& channel) {
    SequencerPatternV2 fallback{};
    fallback.length = channel.sequencer.length;
    fallback.rotation = channel.sequencer.rotation;
    fallback.gates = {{channel.sequencer.pattern, 0ULL}};
    row = buildSequencerV2MenuRow(rowIndex, 0U, fallback);
}

/** Fills one row of the SequencerPattern settings page. */
void fillSequencerPatternRow(MenuRow& row, const std::uint8_t rowIndex) {
    constexpr text::TextId kLabels[] = {
        text::TextId::Invert,
        text::TextId::Clear,
        text::TextId::FillAlternate,
        text::TextId::Copy,
        text::TextId::Paste};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
}

/** Fills one row of the DividerBank settings page. */
void fillDividerBankRow(MenuRow& row, const std::uint8_t rowIndex, const ClockState& state) {
    constexpr text::TextId kLabels[] = {text::TextId::Mode, text::TextId::Bank, text::TextId::Gate};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
    if (rowIndex == 0U) {
        copyText(row.value, sizeof(row.value), text::TextId::ModeDividerLong);
    } else if (rowIndex == 1U) {
        const text::TextId bankText = state.dividerBank.bank == DividerBank::PowersOfTwo
            ? text::TextId::BankPowers
            : (state.dividerBank.bank == DividerBank::Integers ? text::TextId::BankIntegers
                                                               : text::TextId::BankPrimes);
        copyText(row.value, sizeof(row.value), bankText);
    } else {
        text::formatInto(
            row.value,
            sizeof(row.value),
            text::get(text::TextId::MillisecondsFormat),
            state.dividerBank.gateLengthMs);
    }
}

/** Fills one row of a global settings page. Returns false when page is not one of them. */
bool fillGlobalPageRow(
    MenuRow& row,
    const SettingsPage page,
    const std::uint8_t rowIndex,
    const ClockState& state,
    const bool highScoreResetAvailable) {
    if (page == SettingsPage::Root) {
        fillRootRow(row, rowIndex, highScoreResetAvailable);
    } else if (page == SettingsPage::Support) {
        fillSupportRow(row);
    } else if (page == SettingsPage::General) {
        fillGeneralRow(row, rowIndex);
    } else if (page == SettingsPage::InputAssignments) {
        fillInputAssignmentsRow(row, rowIndex, state);
    } else if (page == SettingsPage::Hardware) {
        fillHardwareRow(row, rowIndex, state);
    } else if (page == SettingsPage::Diagnostics) {
        constexpr text::TextId kLabels[] = {text::TextId::Inputs, text::TextId::Outputs};
        copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
        copyText(row.value, sizeof(row.value), text::TextId::Arrow);
    } else if (page == SettingsPage::Master) {
        fillMasterRow(row, rowIndex, state);
    } else if (page == SettingsPage::Sync) {
        row = buildSyncMenuRow(rowIndex, state);
    } else if (page == SettingsPage::Preferences) {
        fillPreferencesRow(row, rowIndex);
    } else if (page == SettingsPage::GrooveEditorMenu) {
        fillGrooveEditorMenuRow(row, rowIndex);
    } else if (page == SettingsPage::GrooveRecordMenu) {
        fillGrooveRecordMenuRow(row, rowIndex);
    } else if (page == SettingsPage::Screensaver) {
        fillScreensaverRow(row, rowIndex, state);
    } else if (page == SettingsPage::Info) {
        fillInfoRow(row, rowIndex);
    } else if (page == SettingsPage::Licenses) {
        fillLicensesRow(row, rowIndex);
    } else if (page == SettingsPage::Updates) {
        copyText(row.label, sizeof(row.label), text::TextId::Updates);
    } else {
        return false;
    }
    return true;
}

/** Fills one row of a channel, mode or groove settings page. Returns false when page is not one of them. */
bool fillChannelPageRow(
    MenuRow& row,
    const SettingsPage page,
    const std::uint8_t rowIndex,
    const std::uint8_t selectedChannel,
    const ClockState& state,
    const ChannelConfig& channel) {
    if (page == SettingsPage::Channel) {
        row = buildChannelMenuRow(rowIndex, channel);
    } else if (page == SettingsPage::ChannelTiming) {
        row = buildChannelTimingMenuRow(rowIndex, channel);
    } else if (page == SettingsPage::ChannelOutput) {
        row = buildChannelOutputMenuRow(rowIndex, channel);
    } else if (page == SettingsPage::Clock) {
        fillClockRow(row, rowIndex, channel);
    } else if (page == SettingsPage::Euclid) {
        fillEuclidRow(row, rowIndex, channel);
    } else if (page == SettingsPage::Sequencer) {
        fillSequencerRow(row, rowIndex, channel);
    } else if (page == SettingsPage::SequencerPattern) {
        fillSequencerPatternRow(row, rowIndex);
    } else if (page == SettingsPage::UnifiedClock) {
        row = buildUnifiedClockMenuRow(rowIndex);
    } else if (page == SettingsPage::UnifiedTiming) {
        row = buildUnifiedTimingMenuRow(rowIndex, state.unifiedClock);
    } else if (page == SettingsPage::UnifiedOutput) {
        row = buildUnifiedOutputMenuRow(rowIndex, state.unifiedClock);
    } else if (page == SettingsPage::Groove) {
        row = buildGrooveMenuRow(rowIndex, selectedChannel, state);
    } else if (page == SettingsPage::DividerBank) {
        fillDividerBankRow(row, rowIndex, state);
    } else {
        return false;
    }
    return true;
}

}  // namespace

MenuRow buildMenuRow(
    const SettingsPage page,
    const std::uint8_t rowIndex,
    const std::uint8_t selectedChannel,
    const ClockState& state,
    const bool highScoreResetAvailable) {
    MenuRow row{};
    const ChannelConfig& channel = state.channels[selectedChannel];

    if (!fillGlobalPageRow(row, page, rowIndex, state, highScoreResetAvailable)) {
        (void)fillChannelPageRow(
            row, page, rowIndex, selectedChannel, state, channel);  // unknown page: empty row
    }
    return row;
}

}  // namespace clockfw::ui
