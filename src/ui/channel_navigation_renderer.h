/**
 * @file channel_navigation_renderer.h
 * @brief Graphical channel, mode, and sequencer-editor screen rendering.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstdint>

#include "domain/clock_types.h"
#include "domain/sequencer_pattern.h"
#include "domain/sequencer_step_metadata.h"
#include "engine/clock_engine.h"
#include "hal/oled_display.h"
#include "ui/pattern_strip_renderer.h"
#include "ui/ui_types.h"

namespace clockfw::ui {

/** @brief Renders graphical navigation and pattern-editing screens. */
class ChannelNavigationRenderer final {
  public:
    /**
     * @brief Constructs the renderer around the shared OLED abstraction.
     * @param display Display HAL receiving all drawing commands.
     */
    explicit ChannelNavigationRenderer(hal::OledDisplay& display);

    /**
     * @brief Draws the compact eight-channel overview and selector.
     * @param state Complete channel configuration used for mode symbols.
     * @param navigation Current candidate channel.
     */
    void renderChannelQuickSelect(const ClockState& state, const NavigationState& navigation);

    /**
     * @brief Draws the horizontally scrolling mode carousel.
     * @param navigation Current selected channel and centered mode cursor.
     */
    void renderModeSelect(const NavigationState& navigation);

    /**
     * @brief Draws the explicit YES/NO confirmation required before changing modes.
     * @param navigation Pending mode function and NO/YES cursor.
     */
    void renderModeChangeConfirm(const NavigationState& navigation);

    /**
     * @brief Draws one 8-step viewport of a channel's 128-step Sequencer 2.0 pattern.
     * @param state Complete channel configuration.
     * @param navigation Current page, channel, and step cursor.
     * @param engineSnapshot Current playback step used by the bottom block indicator.
     * @param activePattern Active Sequencer 2.0 pattern; nullptr shows the legacy pattern of
     *        the selected channel.
     * @param activeSlot Zero-based active pattern slot; shown one-based.
     * @param metadata Packed step expression of the pattern, or nullptr to omit it.
     */
    void renderSequencerEditor(
        const ClockState& state,
        const NavigationState& navigation,
        const engine::EngineSnapshot& engineSnapshot,
        const SequencerPatternV2* activePattern = nullptr,
        std::uint8_t activeSlot = 0U,
        const std::array<SequencerStepMetadataWord, kSequencerMaximumSteps>* metadata = nullptr);

    /**
     * @brief Draws the five-row detail editor for one selected Sequencer step.
     * @param state Complete channel configuration.
     * @param navigation Selected channel, page, step cursor and row cursor.
     * @param activePattern Active Sequencer 2.0 pattern.
     * @param activeSlot Zero-based active pattern slot; shown one-based.
     * @param metadata Decoded expression of the selected step.
     */
    void renderSequencerStepEditor(
        const ClockState& state,
        const NavigationState& navigation,
        const SequencerPatternV2& activePattern,
        std::uint8_t activeSlot,
        const SequencerStepMetadata& metadata);

  private:
    /**
     * @brief Draws the simplified overview shown while a global operating mode is active.
     * @param state Configuration with the global operating mode.
     */
    void renderGlobalModeOverview(const ClockState& state);

    /**
     * @brief Draws the disabled-channel pictogram used by OFF mode selection.
     * @param x Left edge, in pixels.
     * @param y Top edge, in pixels.
     * @param color Drawing color.
     */
    void drawOffModeIcon(std::int16_t x, std::int16_t y, hal::PixelColor color);

    /**
     * @brief Draws the square-wave pictogram used by CLOCK mode selection.
     * @param x Left edge, in pixels.
     * @param y Top edge, in pixels.
     * @param color Drawing color.
     */
    void drawClockModeIcon(std::int16_t x, std::int16_t y, hal::PixelColor color);

    /**
     * @brief Draws the radial hit pictogram used by EUCLID mode selection.
     * @param centerX Centre X, in pixels.
     * @param centerY Centre Y, in pixels.
     * @param color Drawing color.
     */
    void drawEuclidModeIcon(std::int16_t centerX, std::int16_t centerY, hal::PixelColor color);

    /**
     * @brief Draws the miniature gate pattern used by SEQ mode selection.
     * @param x Left edge, in pixels.
     * @param y Top edge, in pixels.
     * @param color Drawing color.
     */
    void drawSequencerModeIcon(std::int16_t x, std::int16_t y, hal::PixelColor color);

    /**
     * @brief Draws the one-clock-to-eight-outputs fan-out pictogram.
     * @param x Left edge, in pixels.
     * @param y Top edge, in pixels.
     * @param color Drawing color.
     */
    void drawUnifiedModeIcon(std::int16_t x, std::int16_t y, hal::PixelColor color);

    /**
     * @brief Draws the multi-rate divider-bank pictogram.
     * @param x Left edge, in pixels.
     * @param y Top edge, in pixels.
     * @param color Drawing color.
     */
    void drawDividerModeIcon(std::int16_t x, std::int16_t y, hal::PixelColor color);

    /**
     * @brief Draws a compact mode pictogram beside one CHn label in the overview.
     * @param state Configuration; a global operating mode overrides the channel mode.
     * @param channelIndex Zero-based channel.
     * @param x Left edge, in pixels.
     * @param y Top edge, in pixels.
     * @param color Drawing color.
     */
    void drawOverviewModeIcon(
        const ClockState& state,
        std::uint8_t channelIndex,
        std::int16_t x,
        std::int16_t y,
        hal::PixelColor color);

    /**
     * @brief Draws one large mode-selector icon for the supplied mode function.
     * @param modeFunction Mode function whose icon is drawn.
     * @param x Left edge of the icon cell, in pixels.
     * @param y Top edge of the icon cell, in pixels.
     * @param color Drawing color.
     */
    void drawModeFunctionIcon(
        ModeFunction modeFunction,
        std::int16_t x,
        std::int16_t y,
        hal::PixelColor color);

    /** Display that receives the drawing commands. */
    hal::OledDisplay& display_;
    /** Draws the Sequencer editor page indicator. */
    PatternStripRenderer patternStripRenderer_;
};

}  // namespace clockfw::ui
