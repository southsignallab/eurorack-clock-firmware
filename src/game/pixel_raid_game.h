/**
 * @file pixel_raid_game.h
 * @brief Hidden original fixed-shooter Easter egg for the 128x64 OLED.
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

/** @brief Runs the boot-only Pixel Raid Easter egg while all Eurorack gate outputs are disabled. */
class PixelRaidGame final {
  public:
    /**
     * @brief Constructs the game around existing display, controls, LED/gate HAL, and score
     *        storage.
     * @param display Display used for all frames; must outlive the game.
     * @param controls Control panel sampled once per loop pass; must outlive the game.
     * @param gateOutputs Gate driver kept low and disabled while the game runs.
     * @param leaderboard Durable ranking store handed to the arcade shell.
     */
    PixelRaidGame(
        hal::OledDisplay& display,
        hal::ControlPanel& controls,
        hal::GateOutputDriver& gateOutputs,
        ArcadeLeaderboardStore& leaderboard);

    /** @brief Runs the game synchronously until the user confirms exit. */
    void run();

#ifdef CLOCK_SIMULATOR
    /** @brief Starts a non-blocking simulator session while keeping all gate source GPIOs muted. */
    void beginForSimulator();

    /**
     * @brief Services one non-blocking simulator game iteration.
     * @param nowMs Current virtual MCU time in milliseconds.
     * @return True while the game remains active.
     */
    bool serviceForSimulator(std::uint32_t nowMs);
#endif

  private:
    friend struct PixelRaidGameTestAccess;

    /** Alien columns in the formation. */
    static constexpr std::uint8_t kAlienColumns = 8U;
    /** Alien rows in the formation. */
    static constexpr std::uint8_t kAlienRows = 3U;
    /** Number of aliens in a full wave. */
    static constexpr std::uint8_t kAlienCount = kAlienColumns * kAlienRows;

    /** @brief Starts a fresh three-life session without touching the durable high score. */
    void resetSession();

    /**
     * @brief Advances game physics from one debounced control sample.
     *
     * Moves the ship and fires on every call; projectiles advance at most once per 40 ms, the
     * formation at its wave-dependent interval.
     * @param controls Control sample of this loop pass.
     * @param nowMs Current system time in milliseconds.
     */
    void update(const hal::ControlSample& controls, std::uint32_t nowMs);

    /** @brief Renders the complete current game state to the OLED framebuffer. */
    void render();

    /** @brief Handles a player shot and alien collision. */
    void updatePlayerShot();

    /**
     * @brief Advances/spawns the enemy projectile and detects player hits.
     *
     * A hit costs a life and calls flashLifeLostLeds(), which blocks for about 280 ms.
     * @param nowMs Current system time in milliseconds; a new shot starts at most every 950 ms.
     */
    void updateEnemyShot(std::uint32_t nowMs);

    /**
     * @brief Advances the alien formation and starts a new faster wave when cleared.
     *
     * The move interval is 420 ms minus 20 ms per wave, at least 120 ms. Reaching the player line
     * ends the run.
     * @param nowMs Current system time in milliseconds.
     */
    void updateAliens(std::uint32_t nowMs);

    /** @brief Flashes all activity LEDs with all gate source GPIOs forced LOW. */
    void flashLifeLostLeds();
    /**
     * @brief True when the lowest living alien row has reached the player line.
     * @return true when the bottom of the lowest living row is at or below the ship's top row.
     */
    bool formationReachedPlayer() const;
    /** @brief Ends the run: GAME OVER, leaderboard and high score. */
    void endRun();

    /**
     * @brief Opens the in-game exit confirmation dialog and returns the selected decision.
     *
     * Blocks in its own display loop until the encoder button is pressed again; host tests return
     * true at once.
     * @return true when YES was selected.
     */
    bool confirmExit();

    /**
     * @brief Returns true when one alive alien contains the supplied point.
     *
     * The hit alien is removed from the formation.
     * @param x Point column, pixels.
     * @param y Point row, pixels.
     * @return true when an alien was hit.
     */
    bool hitAlien(std::int16_t x, std::int16_t y);

    /**
     * @brief Returns the x coordinate of a deterministic alive alien column for enemy fire.
     *
     * Advances the xorshift32 state to pick the first column searched.
     * @return Centre column of the chosen alien, pixels; 64 when no alien is alive.
     */
    std::int16_t chooseEnemyShotX();

    /**
     * @brief Draws one original compact alien glyph; no third-party game artwork is used.
     * @param x Left column of the 5x3 glyph, pixels.
     * @param y Top row of the glyph, pixels.
     */
    void drawAlien(std::int16_t x, std::int16_t y) const;

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

    /** Alien alive flags, row-major (row * kAlienColumns + column). */
    std::array<bool, kAlienCount> aliens_{};
    /** Left edge of the ship, pixels. */
    std::int16_t playerX_ = 61;
    /** Left edge of the formation, pixels. */
    std::int16_t alienOffsetX_ = 7;
    /** Top edge of the formation, pixels; grows by 2 at each edge reversal. */
    std::int16_t alienOffsetY_ = 13;
    /** Formation direction, +1 right or -1 left; 2 pixels per move. */
    std::int8_t alienDirection_ = 1;
    /** Remaining lives. */
    std::uint8_t lives_ = 3U;
    /** Current wave, from 1; scales score and formation speed. */
    std::uint8_t wave_ = 1U;
    /** Score of the current run. */
    std::uint32_t score_ = 0U;
    /** xorshift32 state for enemy column choice; not reset per session. */
    std::uint32_t randomState_ = 0x13579BDFUL;
    /** Time of the last formation move, ms. */
    std::uint32_t lastAlienMoveAtMs_ = 0U;
    /** Time the last enemy shot started, ms. */
    std::uint32_t lastEnemyShotAtMs_ = 0U;
    /** Time of the last projectile step, ms. */
    std::uint32_t lastProjectileUpdateAtMs_ = 0U;
#ifdef CLOCK_SIMULATOR
    /** Time of the last simulator frame, ms. */
    std::uint32_t lastSimulatorFrameAtMs_ = 0U;
#endif
    /** Encoder-button long press that opens the exit dialog. */
    LongPressDetector exitHold_{};
    /** Player shot is in flight. */
    bool playerShotActive_ = false;
    /** Player shot column, pixels. */
    std::int16_t playerShotX_ = 0;
    /** Player shot top row, pixels; rises 2 pixels per step. */
    std::int16_t playerShotY_ = 0;
    /** Enemy shot is in flight. */
    bool enemyShotActive_ = false;
    /** Enemy shot column, pixels. */
    std::int16_t enemyShotX_ = 0;
    /** Enemy shot top row, pixels; falls 2 pixels per step. */
    std::int16_t enemyShotY_ = 0;
    /** Exit was confirmed. */
    bool exitRequested_ = false;
    /** All lives are used or the formation landed; the score is final. */
    bool gameOver_ = false;
};

}  // namespace clockfw::game
