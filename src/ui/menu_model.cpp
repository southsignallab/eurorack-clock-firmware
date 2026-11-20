/**
 * @file menu_model.cpp
 * @brief Static settings-page metadata and option-index lookups (rows: menu_model_rows.cpp).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "ui/menu_model.h"
#include "ui/menu_model_channel.h"

#include "domain/clock_options.h"
#include "ui_text.h"

namespace clockfw::ui {

const char* settingsPageTitle(const SettingsPage page) {
    switch (page) {
        case SettingsPage::General:
            return text::get(text::TextId::GeneralSettings);
        case SettingsPage::InputAssignments:
            return text::get(text::TextId::Inputs);
        case SettingsPage::Hardware:
            return text::get(text::TextId::Hardware);
        case SettingsPage::Diagnostics:
            return text::get(text::TextId::Diagnostics);
        case SettingsPage::DiagnosticsInputs:
            return text::get(text::TextId::Inputs);
        case SettingsPage::DiagnosticsOutputs:
            return text::get(text::TextId::Outputs);
        case SettingsPage::Master:
            return text::get(text::TextId::ModeClockLong);
        case SettingsPage::Sync:
            return text::get(text::TextId::InputConfig);
        case SettingsPage::Preferences:
            return text::get(text::TextId::Presets);
        case SettingsPage::Screensaver:
            return text::get(text::TextId::Screensaver);
        case SettingsPage::Info:
            return text::get(text::TextId::Info);
        case SettingsPage::Support:
            return text::get(text::TextId::SupportClock);
        case SettingsPage::Licenses:
            return text::get(text::TextId::Licenses);
        case SettingsPage::Updates:
            return text::get(text::TextId::Updates);
        case SettingsPage::Channel:
            return text::get(text::TextId::Channel);
        case SettingsPage::ChannelTiming:
        case SettingsPage::UnifiedTiming:
            return text::get(text::TextId::Timing);
        case SettingsPage::ChannelOutput:
        case SettingsPage::UnifiedOutput:
            return text::get(text::TextId::Output);
        case SettingsPage::Clock:
            return text::get(text::TextId::ModeClockLong);
        case SettingsPage::Euclid:
            return text::get(text::TextId::ModeEuclidLong);
        case SettingsPage::Sequencer:
            return text::get(text::TextId::ModeSequencerLong);
        case SettingsPage::SequencerPattern:
            return text::get(text::TextId::Pattern);
        case SettingsPage::UnifiedClock:
            return text::get(text::TextId::ModeUnifiedLong);
        case SettingsPage::Groove:
            return text::get(text::TextId::Groove);
        case SettingsPage::GrooveEditorMenu:
            return text::get(text::TextId::GrooveMenu);
        case SettingsPage::GrooveRecordMenu:
            return text::get(text::TextId::GrooveRecordMenu);
        case SettingsPage::DividerBank:
            return text::get(text::TextId::ModeDividerLong);
        case SettingsPage::Root:
        default:
            return text::get(text::TextId::RootSettings);
    }
}

std::uint8_t settingsPageItemCount(
    const SettingsPage page,
    const ChannelMode mode,
    const bool highScoreResetAvailable) {
    switch (page) {
        case SettingsPage::Root:
            return highScoreResetAvailable ? 7U : 6U;
        case SettingsPage::General:
            return 5U;
        case SettingsPage::InputAssignments:
            return 3U;
        case SettingsPage::Hardware:
            return 2U;
        case SettingsPage::Diagnostics:
            return 2U;
        case SettingsPage::DiagnosticsInputs:
        case SettingsPage::DiagnosticsOutputs:
            return 0U;
        case SettingsPage::Master:
            return 6U;
        case SettingsPage::Sync:
            return 9U;
        case SettingsPage::Preferences:
            return 4U;
        case SettingsPage::Screensaver:
            return 4U;
        case SettingsPage::Info:
            return 6U;
        case SettingsPage::Support:
            return 1U;
        case SettingsPage::Licenses:
            return 7U;
        case SettingsPage::Updates:
            return 1U;
        case SettingsPage::Channel:
            return channelMenuItemCount(mode);
        case SettingsPage::ChannelTiming:
            return 5U;
        case SettingsPage::ChannelOutput:
            return 5U;
        case SettingsPage::Clock:
            return 2U;
        case SettingsPage::Euclid:
            return 3U;
        case SettingsPage::Sequencer:
            return 6U;
        case SettingsPage::SequencerPattern:
            return 5U;
        case SettingsPage::UnifiedClock:
            return 3U;
        case SettingsPage::UnifiedTiming:
            return 6U;
        case SettingsPage::UnifiedOutput:
            return 2U;
        case SettingsPage::Groove:
            return 8U;
        case SettingsPage::GrooveEditorMenu:
            return 4U;
        case SettingsPage::GrooveRecordMenu:
            return 7U;
        case SettingsPage::DividerBank:
            return 3U;
        default:
            return 0U;
    }
}

bool isChannelSettingsPage(const SettingsPage page) {
    return page == SettingsPage::Channel || page == SettingsPage::ChannelTiming ||
        page == SettingsPage::ChannelOutput || page == SettingsPage::Clock ||
        page == SettingsPage::Euclid || page == SettingsPage::Sequencer ||
        page == SettingsPage::SequencerPattern;
}

std::size_t findRateOptionIndex(const CommonChannelSettings& settings) {
    for (std::size_t index = 0U; index < kRateOptions.size(); ++index) {
        if (kRateOptions[index].mode == settings.rate.mode &&
            kRateOptions[index].factor == settings.rate.factor) {
            return index;
        }
    }
    return 9U;
}

std::size_t findGateLengthOptionIndex(const std::uint16_t gateLengthMs) {
    for (std::size_t index = 0U; index < kGateLengthOptionsMs.size(); ++index) {
        if (kGateLengthOptionsMs[index] == gateLengthMs) {
            return index;
        }
    }
    return 3U;
}

std::size_t findBeatUnitOptionIndex(const std::uint8_t beatUnit) {
    for (std::size_t index = 0U; index < kBeatUnitOptions.size(); ++index) {
        if (kBeatUnitOptions[index] == beatUnit) {
            return index;
        }
    }
    return 1U;
}

}  // namespace clockfw::ui
