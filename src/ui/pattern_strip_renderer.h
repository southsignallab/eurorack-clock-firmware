/**
 * @file pattern_strip_renderer.h
 * @brief Compact playback pattern visualizations for Euclid and sequencer modes.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

#include "domain/clock_types.h"
#include "domain/sequencer_pattern.h"
#include "hal/oled_display.h"

namespace clockfw::ui {

/** @brief Draws non-interactive rhythm strips shared by performance and editor screens. */
class PatternStripRenderer final {
  public:
    /**
     * @brief Constructs the helper around the shared OLED framebuffer.
     * @param display Display HAL receiving all drawing primitives.
     */
    explicit PatternStripRenderer(hal::OledDisplay& display);

    /**
     * @brief Draws the active sixteen-step Euclidean playback block.
     * @param settings Euclidean Steps/Hits/Rotation configuration.
     * @param currentStep Zero-based currently sounding step.
     * @param topY Top pixel row of the pattern marks.
     */
    void drawEuclidPattern(
        const EuclidSettings& settings,
        std::uint8_t currentStep,
        std::int16_t topY);

    /**
     * @brief Draws the currently playing 16-step sequencer block.
     * @param settings Sequencer length, rotation, and binary pattern.
     * @param currentStep Zero-based currently sounding step.
     * @param topY Top pixel row of the 16 step cells.
     */
    void drawSequencerPlaybackBlock(
        const SequencerSettings& settings,
        std::uint8_t currentStep,
        std::int16_t topY);

    /**
     * @brief Draws the currently playing 16-step block from a Sequencer 2.0 pattern.
     * @param pattern Sequencer 2.0 pattern; length 0 is treated as 1.
     * @param currentStep Zero-based currently sounding step; wrapped to the pattern length.
     * @param topY Top pixel row of the 16 step cells.
     */
    void drawSequencerPlaybackBlock(
        const SequencerPatternV2& pattern,
        std::uint8_t currentStep,
        std::int16_t topY);

    /**
     * @brief Draws 8-step editor-page progress for patterns up to 128 steps.
     * @param sequenceLength Pattern length in steps; nothing is drawn up to 8 steps.
     * @param activeStep Zero-based step that selects the highlighted page.
     */
    void drawSequencerEditorPageIndicator(std::uint8_t sequenceLength, std::uint8_t activeStep);

    /**
     * @brief Draws 16-step block indicators at the two bottom OLED rows.
     * @param sequenceLength Active sequence length in steps.
     * @param activeStep Zero-based playback step used to select the active block.
     *
     * Inactive blocks use a one-pixel line on row 63. The active block is a
     * two-pixel-high line on rows 62-63, making playback position visible without
     * consuming normal content rows.
     */
    void drawSequencerBlockIndicator(std::uint8_t sequenceLength, std::uint8_t activeStep);

  private:
    /**
     * @brief Draws shared 16-step block progress for long Euclid/Sequencer patterns.
     * @param sequenceLength Pattern length in steps; nothing is drawn up to @p blockSize.
     * @param activeStep Zero-based step that selects the highlighted block.
     * @param blockSize Steps per block; 0 draws nothing.
     */
    void drawPatternBlockIndicator(
        std::uint8_t sequenceLength,
        std::uint8_t activeStep,
        std::uint8_t blockSize);

    /**
     * @brief Draws a two-pixel-high underline for one current pattern slot.
     * @param slotX Left edge of the slot, in pixels.
     * @param slotWidth Slot width in pixels; the underline is one pixel shorter, at least 1.
     */
    void drawCurrentStepUnderline(std::int16_t slotX, std::int16_t slotWidth);

    /** Display that receives the drawing commands. */
    hal::OledDisplay& display_;
};

}  // namespace clockfw::ui
