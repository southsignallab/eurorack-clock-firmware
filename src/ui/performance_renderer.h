/**
 * @file performance_renderer.h
 * @brief Rendering of the normal performance screen and compact playback status line.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

#include "domain/clock_types.h"
#include "domain/sequencer_pattern.h"
#include "engine/clock_engine.h"
#include "hal/oled_display.h"
#include "services/custom_groove_store.h"
#include "ui/pattern_strip_renderer.h"
#include "ui/ui_types.h"

namespace clockfw::ui {

/** @brief Renders the calm, tempo-focused performance view used during normal operation. */
class PerformanceRenderer final {
  public:
    /**
     * @brief Constructs the renderer around the shared OLED abstraction.
     * @param display Display HAL receiving all drawing commands.
     * @param customGrooveStore Optional Custom Groove library for the name of a CUSTOM groove.
     */
    explicit PerformanceRenderer(
        hal::OledDisplay& display,
        const services::CustomGrooveStore* customGrooveStore = nullptr);

    /**
     * @brief Draws the complete performance screen and presents the framebuffer.
     * @param state Current user configuration and transport state.
     * @param navigation UI state identifying the currently focused channel.
     * @param engineSnapshot Read-only real-time timing information.
     * @param activeSequencerPattern Active Sequencer 2.0 pattern of the focused channel, or
     *        nullptr when none is available.
     */
    void render(
        const ClockState& state,
        const NavigationState& navigation,
        const engine::EngineSnapshot& engineSnapshot,
        const SequencerPatternV2* activeSequencerPattern = nullptr);

  private:
    /**
     * @brief Draws PLAY, PAUSE, or STOP right-aligned in the status line.
     * @param transport Transport state to show.
     */
    void drawTransport(TransportState transport);

    /**
     * @brief Draws centered tempo/status text using the requested native-resolution font.
     * @param text NUL-terminated text to draw.
     * @param font Font to draw with.
     * @param topY Top edge of the text, in pixels.
     */
    void drawTempoText(const char* text, hal::DisplayFont font, std::int16_t topY);

    /**
     * @brief Draws channel/global operating context after the role badge.
     * @param channelIndex Zero-based focused channel; shown one-based.
     * @param operatingMode Operating mode; One Clock and Divider Bank show a fixed label.
     * @param mode Mode of the focused channel in the Independent operating mode.
     * @param leftEdgeX Left edge of the text, in pixels.
     */
    void drawChannelStatus(
        std::uint8_t channelIndex,
        OperatingMode operatingMode,
        ChannelMode mode,
        std::int16_t leftEdgeX);

    /**
     * @brief Draws the master/slave badge at the far left and optional external-lock icon.
     * @param source Configured clock source.
     * @param externalLocked The engine reports a lock to the external clock.
     */
    void drawClockRoleStatus(ClockSource source, bool externalLocked);

    /**
     * @brief Draws the eight effective divider-bank rates in a compact 2x4 slot grid.
     * @param state Configuration holding the divider-bank rates.
     */
    void drawDividerBankSlots(const ClockState& state);

    /**
     * @brief Draws a graphical multiply/divide symbol followed by the integer factor.
     * @param x Left edge of the slot, in pixels.
     * @param y Top edge of the slot, in pixels.
     * @param rate Rate to show.
     */
    void drawDividerRateSlot(std::int16_t x, std::int16_t y, const RateSettings& rate);

    /**
     * @brief Draws a compact 5x7 padlock icon.
     * @param x Left edge of the icon, in pixels.
     * @param y Top edge of the icon, in pixels.
     */
    void drawLockIcon(std::int16_t x, std::int16_t y);

    /**
     * @brief Draws the active groove preset at the requested lower-screen position.
     * @param settings Groove settings to show.
     * @param y Top edge of the text line, in pixels.
     */
    void drawGrooveStatus(const GrooveSettings& settings, std::int16_t y);

    /**
     * @brief Draws the ONE CLOCK humanize status icon.
     * @param x Left edge of the icon, in pixels.
     * @param y Top edge of the icon, in pixels.
     */
    void drawHumanizeIcon(std::int16_t x, std::int16_t y);

    /**
     * @brief Draws one 8x8 shrinking Tap Tempo feedback frame beside the BPM.
     * @param x Left edge of the 8x8 slot, in pixels.
     * @param y Top edge of the 8x8 slot, in pixels.
     * @param frame Animation frame 1..4; 0 and larger values draw nothing.
     */
    void drawTapIndicator(std::int16_t x, std::int16_t y, std::uint8_t frame);

    /**
     * @brief Draws the centered static silent count-in popover and meter progress.
     * @param state Current configuration.
     * @param engineSnapshot Engine snapshot with the Pre-Count state.
     */
    void drawPreCountOverlay(const ClockState& state, const engine::EngineSnapshot& engineSnapshot);

    /**
     * @brief Applies transient Performance overlays and presents the completed frame.
     * @param state Current configuration.
     * @param navigation UI state with the transient notices.
     * @param engineSnapshot Engine snapshot with the Pre-Count state.
     */
    void presentPerformanceFrame(
        const ClockState& state,
        const NavigationState& navigation,
        const engine::EngineSnapshot& engineSnapshot);

    /** @brief Draws the transient EXTERNAL CLOCK notice over the tempo area. */
    void drawExternalTempoNotice();

    /**
     * @brief Draws one centered full-width notice banner across the tempo area.
     * @param notice NUL-terminated notice text.
     */
    void drawNoticeBanner(const char* notice);

    /**
     * @brief Returns the one-character O/C/E/S status abbreviation for one channel mode.
     * @param mode Channel mode.
     * @return 'O', 'E' or 'S' for OFF, EUCLID and SEQ; 'C' for CLOCK and any other value.
     */
    static char statusModeCharacter(ChannelMode mode);

    /** Display that receives the drawing commands. */
    hal::OledDisplay& display_;
    /** Optional Custom Groove library for CUSTOM groove names. */
    const services::CustomGrooveStore* customGrooveStore_ = nullptr;
    /** Draws the Euclid/Sequencer pattern strip. */
    PatternStripRenderer patternStripRenderer_;
};

}  // namespace clockfw::ui
