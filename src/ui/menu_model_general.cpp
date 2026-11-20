/**
 * @file menu_model_general.cpp
 * @brief Rows of the screensaver, information and license settings pages.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "ui/menu_model_general.h"
#include "ui/menu_model_channel.h"

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

/** Returns the localized name of a screensaver mode. */
text::TextId screensaverModeText(const ScreensaverMode mode) {
    text::TextId modeText = text::TextId::ScreensaverNone;
    switch (mode) {
        case ScreensaverMode::Fractal:
            modeText = text::TextId::ScreensaverFractal;
            break;
        case ScreensaverMode::Orbit:
            modeText = text::TextId::ScreensaverOrbit;
            break;
        case ScreensaverMode::Plug:
            modeText = text::TextId::ScreensaverPlug;
            break;
        case ScreensaverMode::Clock:
            modeText = text::TextId::ScreensaverClock;
            break;
        case ScreensaverMode::Heartbeat:
            modeText = text::TextId::ScreensaverHeartbeat;
            break;
        case ScreensaverMode::Acid:
            modeText = text::TextId::ScreensaverAcid;
            break;
        case ScreensaverMode::Spectrum:
            modeText = text::TextId::ScreensaverSpectrum;
            break;
        case ScreensaverMode::Field:
            modeText = text::TextId::ScreensaverField;
            break;
        case ScreensaverMode::Blox:
            modeText = text::TextId::ScreensaverBlox;
            break;
        case ScreensaverMode::Matrix:
            modeText = text::TextId::ScreensaverMatrix;
            break;
        case ScreensaverMode::CubeCover:
            modeText = text::TextId::ScreensaverCubeCover;
            break;
        case ScreensaverMode::MakeMusic:
            modeText = text::TextId::ScreensaverMakeMusic;
            break;
        case ScreensaverMode::Labyrinth:
            modeText = text::TextId::ScreensaverLabyrinth;
            break;
        case ScreensaverMode::Starfield:
            modeText = text::TextId::ScreensaverStarfield;
            break;
        case ScreensaverMode::Fireworks:
            modeText = text::TextId::ScreensaverFireworks;
            break;
        case ScreensaverMode::None:
        default:
            break;
    }
    return modeText;
}

}  // namespace

/** Fills one row of the Screensaver settings page. */
void fillScreensaverRow(MenuRow& row, const std::uint8_t rowIndex, const ClockState& state) {
    constexpr text::TextId kLabels[] = {
        text::TextId::Mode,
        text::TextId::StartAfter,
        text::TextId::DimAfter,
        text::TextId::OffAfter};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
    if (rowIndex == 0U) {
        const text::TextId modeText = screensaverModeText(state.display.screensaverMode);
        copyText(row.value, sizeof(row.value), modeText);
    } else {
        const std::uint8_t minutes = rowIndex == 1U
            ? state.display.screensaverAfterMinutes
            : (rowIndex == 2U ? state.display.dimAfterMinutes : state.display.offAfterMinutes);
        text::formatInto(
            row.value,
            sizeof(row.value),
            text::get(text::TextId::MinutesFormat),
            static_cast<unsigned>(minutes));
    }
}

/** Fills one row of the Info settings page. */
void fillInfoRow(MenuRow& row, const std::uint8_t rowIndex) {
    constexpr text::TextId kLabels[] = {
        text::TextId::Product,
        text::TextId::Firmware,
        text::TextId::Author,
        text::TextId::Licenses,
        text::TextId::Updates,
        text::TextId::FactoryReset};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
    if (rowIndex == 0U) {
        copyText(row.value, sizeof(row.value), text::TextId::FirmwareName);
        row.expandableInformation = true;
    } else if (rowIndex == 1U) {
        text::formatInto(row.value, sizeof(row.value), "%s", CLOCK_FIRMWARE_VERSION);
        row.expandableInformation = true;
    } else if (rowIndex == 2U) {
        copyText(row.value, sizeof(row.value), text::TextId::AuthorName);
        row.expandableInformation = true;
    } else {
        copyText(row.value, sizeof(row.value), text::TextId::Arrow);
    }
}

/** Fills one row of the Licenses settings page. */
void fillLicensesRow(MenuRow& row, const std::uint8_t rowIndex) {
    constexpr text::TextId kLabels[] = {
        text::TextId::License,
        text::TextId::Framework,
        text::TextId::CoreLicense,
        text::TextId::HalLicense,
        text::TextId::CmsisLicense,
        text::TextId::Font,
        text::TextId::FontLicense};
    copyText(row.label, sizeof(row.label), kLabels[rowIndex]);
    if (rowIndex == 0U) {
        copyText(row.value, sizeof(row.value), text::TextId::FirmwareLicenseShort);
    } else if (rowIndex == 1U) {
        copyText(row.value, sizeof(row.value), text::TextId::FrameworkName);
    } else if (rowIndex == 2U) {
        copyText(row.value, sizeof(row.value), text::TextId::CubeLicenseShort);
    } else if (rowIndex == 3U) {
        copyText(row.value, sizeof(row.value), text::TextId::BsdShort);
    } else if (rowIndex == 4U) {
        copyText(row.value, sizeof(row.value), text::TextId::ApacheShort);
    } else if (rowIndex == 5U) {
        copyText(row.value, sizeof(row.value), text::TextId::FontName);
    } else {
        copyText(row.value, sizeof(row.value), text::TextId::ApacheShort);
    }
    row.expandableInformation = true;
}

}  // namespace clockfw::ui
