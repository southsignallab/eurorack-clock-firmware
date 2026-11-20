/**
 * @file screensaver_renderer.h
 * @brief STOP-mode OLED idle-animation renderer.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstdint>
#include <limits>

#include "domain/clock_types.h"
#include "hal/oled_display.h"

namespace clockfw::ui {

/** @brief Renders low-frequency idle effects without participating in clock timing. */
class ScreensaverRenderer final {
  public:
    /**
     * @brief Constructs the renderer around the display HAL.
     * @param display Display that receives the frames; must outlive the renderer.
     */
    explicit ScreensaverRenderer(hal::OledDisplay& display);

    /**
     * @brief Renders one frame of the selected STOP-mode idle effect.
     * @param mode Configured screensaver mode.
     * @param frameIndex Monotonic animation frame number.
     */
    void render(ScreensaverMode mode, std::uint32_t frameIndex);

  private:
    /**
     * @brief Renders a progressively revealed fixed-point Barnsley-fern crop.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderFractal(std::uint32_t frameIndex);

    /** @brief Selects a different curated fern crop for the next growth cycle. */
    void selectNextFernViewport();

    /**
     * @brief Renders a sparse, slowly moving orbital line animation.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderOrbit(std::uint32_t frameIndex);

    /**
     * @brief Renders a damped plucked-string animation.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderPlug(std::uint32_t frameIndex);

    /**
     * @brief Renders eight independently phased digital clock traces.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderClock(std::uint32_t frameIndex);

    /**
     * @brief Renders a cyclic pulsating heart outline.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderHeartbeat(std::uint32_t frameIndex);

    /**
     * @brief Renders a gravity-influenced bouncing and rotating acid smiley.
     * @param frameIndex Monotonic animation frame number; a smaller value than the last one
     *        restarts the motion.
     */
    void renderAcid(std::uint32_t frameIndex);

    /**
     * @brief Renders a synthetic WinAmp-style spectrum analyser with peak hold/decay.
     * @param frameIndex Monotonic animation frame number; a value not above the last one
     *        restarts the bands.
     */
    void renderSpectrum(std::uint32_t frameIndex);

    /**
     * @brief Renders animated metaball-like scalar-field isocontours.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderField(std::uint32_t frameIndex);

    /**
     * @brief Renders falling triangle-built bodies that accumulate on a simple cell stack.
     * @param frameIndex Monotonic animation frame number; a smaller value than the last one
     *        clears the stack.
     */
    void renderBlox(std::uint32_t frameIndex);

    /**
     * @brief Renders original monochrome digital-rain columns with procedural glyphs.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderMatrix(std::uint32_t frameIndex);

    /**
     * @brief Covers the display with 8x8 cube tiles using changing traversal patterns.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderCubeCover(std::uint32_t frameIndex);

    /**
     * @brief Builds the MAKE·MUSIC·NOT·WAR message one glyph at a time.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderMakeMusic(std::uint32_t frameIndex);

    /**
     * @brief Draws a succession of deterministic random perfect mazes.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderLabyrinth(std::uint32_t frameIndex);

    /**
     * @brief Renders a three-depth parallax starfield.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderStarfield(std::uint32_t frameIndex);

    /**
     * @brief Launches a pixel rocket and renders a compact upper-screen burst.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderFireworks(std::uint32_t frameIndex);

    /** Marks a "last frame" field as not yet rendered, so the next frame restarts the effect. */
    static constexpr std::uint32_t kInvalidFrameIndex = std::numeric_limits<std::uint32_t>::max();

    /** Display that receives the frames. */
    hal::OledDisplay& display_;
    /** Pseudo-random state for choosing the next fern crop. */
    std::uint32_t fernViewportRandomState_ = 0xA341316CU;
    /** Frame index of the last fern frame. */
    std::uint32_t lastFractalFrameIndex_ = kInvalidFrameIndex;
    /** Index of the current fern crop. */
    std::uint8_t fernViewportIndex_ = 0U;
    /** Frame index up to which the smiley motion has been simulated. */
    std::uint32_t lastAcidFrameIndex_ = kInvalidFrameIndex;
    /** Smiley centre X in pixels, Q8 fixed point (1/256 pixel). */
    std::int32_t acidXQ8_ = 28 * 256;
    /** Smiley centre Y in pixels, Q8 fixed point (1/256 pixel). */
    std::int32_t acidYQ8_ = 18 * 256;
    /** Smiley X velocity in 1/256 pixel per simulation step. */
    std::int32_t acidVelocityXQ8_ = 330;
    /** Smiley Y velocity in 1/256 pixel per simulation step. */
    std::int32_t acidVelocityYQ8_ = -390;
    /** Smiley rotation in 1/32 turns, Q8 fixed point. */
    std::int32_t acidAngleQ8_ = 0;
    /** Smiley rotation speed per simulation step, same unit as acidAngleQ8_. */
    std::int32_t acidAngularVelocityQ8_ = 110;
    /** Frame index up to which the spectrum bands have been simulated. */
    std::uint32_t lastSpectrumFrameIndex_ = kInvalidFrameIndex;
    /** Pseudo-random state of the spectrum noise. */
    std::uint32_t spectrumRandomState_ = 0x51EC7A2DU;
    /** Current bar height of each of the 16 bands, in pixels. */
    std::array<std::uint8_t, 16U> spectrumLevels_{};
    /** Peak marker height of each band, in pixels. */
    std::array<std::uint8_t, 16U> spectrumPeaks_{};
    /** Remaining simulation steps before each peak marker starts to fall. */
    std::array<std::uint8_t, 16U> spectrumPeakHold_{};
    /** Frame index of the last Blox frame. */
    std::uint32_t lastBloxFrameIndex_ = kInvalidFrameIndex;
    /** Pseudo-random state for Blox shape and column choice. */
    std::uint32_t bloxRandomState_ = 0xB10C5EEDU;
    /** Settled cells, one bit per column for each of the 8 rows. */
    std::array<std::uint16_t, 8U> bloxRows_{};
    /** Shape index of the falling body. */
    std::int8_t bloxShape_ = 0;
    /** Column of the falling body, in cells. */
    std::int8_t bloxCellX_ = 6;
    /** Row of the falling body, in cells; negative above the top. */
    std::int8_t bloxCellY_ = -2;
    /** Frames the full stack stays visible before it is cleared. */
    std::uint8_t bloxFullHoldFrames_ = 0U;
    /** A body is falling. */
    bool bloxActive_ = false;
};

}  // namespace clockfw::ui
