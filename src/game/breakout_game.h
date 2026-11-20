/**
 * @file breakout_game.h
 * @brief Boot-only monochrome brick-breaker Easter egg.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <array>
#include <cstdint>

#include "game/arcade_leaderboard_store.h"
#include "game/arcade_shell.h"
#include "hal/control_panel.h"
#include "hal/gate_output_driver.h"
#include "hal/oled_display.h"

namespace clockfw::game {

/** @brief Encoder-controlled brick breaker with variable angles and original power-up mechanics. */
class BreakoutGame final {
  public:
    /**
     * @brief Constructs the game around the shared display, controls, and safe gate-output HAL.
     * @param display Display used for all frames; must outlive the game.
     * @param controls Control panel sampled once per loop pass; must outlive the game.
     * @param gateOutputs Gate driver kept low and disabled while the game runs.
     * @param leaderboard Durable ranking store handed to the arcade shell.
     */
    BreakoutGame(
        hal::OledDisplay& display,
        hal::ControlPanel& controls,
        hal::GateOutputDriver& gateOutputs,
        ArcadeLeaderboardStore& leaderboard);
    /** @brief Runs the selected boot game synchronously until exit. */
    void run();
#ifdef CLOCK_SIMULATOR
    /** @brief Starts a non-blocking simulator game session. */
    void beginForSimulator();
    /**
     * @brief Services one simulator frame and returns true while the game remains active.
     * @param nowMs Current system time in milliseconds.
     * @return true while no exit was requested.
     */
    bool serviceForSimulator(std::uint32_t nowMs);
#endif

  private:
    friend struct BreakoutGameTestAccess;
    /** Brick columns per row. */
    static constexpr std::uint8_t kColumns = 12U;
    /** Brick rows. */
    static constexpr std::uint8_t kRows = 4U;
    /** Number of bricks in a full wall. */
    static constexpr std::uint8_t kBrickCount = kColumns * kRows;

    /** @brief Temporary paddle modifier carried by a falling extra. */
    enum class ExtraType : std::uint8_t {
        None,      ///< No modifier; default paddle width and step.
        SmallBat,  ///< Narrower paddle.
        LargeBat,  ///< Wider paddle.
        FastBat    ///< Larger paddle step per encoder detent.
    };
    /** @brief Power-up falling from a destroyed brick; at most one at a time. */
    struct FallingExtra final {
        ExtraType type = ExtraType::None;  ///< Modifier applied when caught.
        std::int16_t x = 0;                ///< Centre column, pixels.
        std::int16_t y = 0;                ///< Centre row, pixels; falls 1 pixel per frame.
        bool active = false;               ///< Extra is falling.
    };

    /** @brief Restores bricks, paddle, effects, and waiting-ball state. */
    void resetSession();
    /** @brief Places the ball back on the paddle awaiting a TAP launch. */
    void resetBall();
    /**
     * @brief Advances paddle input, variable-angle physics, bricks, and falling extras.
     *
     * Input is taken on every call; physics advances at most once per 35 ms frame interval.
     * Clearing all bricks awards a level bonus and refills the wall.
     * @param controls Control sample of this loop pass.
     * @param nowMs Current system time in milliseconds.
     */
    void update(const hal::ControlSample& controls, std::uint32_t nowMs);
    /**
     * @brief Advances one collision-safe substep of ball motion.
     *
     * Moves the ball by at most one pixel per axis, then resolves walls, paddle, at most one
     * brick, and a lost ball.
     * @param nowMs Current system time in milliseconds; currently unused.
     * @param moveX Move one pixel horizontally in the direction of velocityX_.
     * @param moveY Move one pixel vertically in the direction of velocityY_.
     */
    void updateBallSubstep(std::uint32_t nowMs, bool moveX, bool moveY);
    /** @brief Applies a paddle-angle response based on the impact point. */
    void bounceFromPaddle();
    /**
     * @brief Occasionally creates one deterministic falling extra at a destroyed brick.
     *
     * Spawns with a chance of one in seven when no extra is falling.
     * @param x Start column, pixels.
     * @param y Start row, pixels.
     */
    void maybeSpawnExtra(std::int16_t x, std::int16_t y);
    /**
     * @brief Advances and applies an active falling extra.
     *
     * Also restores the default paddle once the active modifier has expired.
     * @param nowMs Current system time in milliseconds.
     */
    void updateExtra(std::uint32_t nowMs);
    /**
     * @brief Applies one temporary paddle modifier.
     * @param type Modifier to apply; replaces any active modifier.
     * @param nowMs Current system time in milliseconds; the modifier lasts 8000 ms from here.
     */
    void applyExtra(ExtraType type, std::uint32_t nowMs);
    /**
     * @brief Advances the local deterministic PRNG used only by this game.
     * @return Next xorshift32 state.
     */
    std::uint32_t nextRandom();
    /** @brief Renders walls, bricks, paddle, ball, extra, and launch hint. */
    void render();

    /** Borrowed display. */
    hal::OledDisplay& display_;
    /** Borrowed control panel. */
    hal::ControlPanel& controls_;
    /** Borrowed gate driver, held low while the game runs. */
    hal::GateOutputDriver& gateOutputs_;
    /** Title, ranking, and game-over flow shared by the arcade titles. */
    ArcadeShell shell_;
    /** Best score shown in the header. */
    LeaderboardEntry highScore_{};
    /** Brick present flags, row-major (row * kColumns + column). */
    std::array<bool, kBrickCount> bricks_{};
    /** Left edge of the paddle, pixels. */
    std::int16_t paddleX_ = 54;
    /** Paddle width, pixels; changed by extras. */
    std::int16_t paddleWidth_ = 20;
    /** Paddle movement per encoder detent, pixels; changed by extras. */
    std::int16_t paddleStep_ = 4;
    /** Ball centre column, pixels. */
    std::int16_t ballX_ = 64;
    /** Ball centre row, pixels. */
    std::int16_t ballY_ = 52;
    /** Horizontal ball speed, pixels per physics frame, -2..2. */
    std::int8_t velocityX_ = 1;
    /** Vertical ball speed, pixels per physics frame, -2..2; negative moves up. */
    std::int8_t velocityY_ = -1;
    /** xorshift32 state. */
    std::uint32_t randomState_ = 0xB8EA4C31U;
    /** Score of the current run, capped at 999999. */
    std::uint32_t score_ = 0U;
    /** Remaining lives. */
    std::uint8_t lives_ = 3U;
    /** Current level, 1..99; scales brick and clear bonuses. */
    std::uint8_t level_ = 1U;
    /** Time of the last physics frame, ms. */
    std::uint32_t lastPhysicsAtMs_ = 0U;
    /** Encoder-button long press that exits the game. */
    LongPressDetector exitHold_{};
    /** Expiry time of activeExtra_, ms. */
    std::uint32_t extraExpiresAtMs_ = 0U;
    /** The falling extra, if any. */
    FallingExtra fallingExtra_{};
    /** Modifier currently applied to the paddle. */
    ExtraType activeExtra_ = ExtraType::None;
#ifdef CLOCK_SIMULATOR
    /** Time of the last simulator frame, ms. */
    std::uint32_t lastSimulatorFrameAtMs_ = 0U;
#endif
    /** Ball has left the paddle after a TAP press. */
    bool ballLaunched_ = false;
    /** All lives are used; the score is final. */
    bool gameOver_ = false;
    /** Long press requested exit from run(). */
    bool exitRequested_ = false;
};

}  // namespace clockfw::game
