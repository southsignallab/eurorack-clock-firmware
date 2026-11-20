/**
 * @file formula1_game.h
 * @brief Boot-only monochrome pseudo-3D lane racer Easter egg.
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

/** @brief Automatic-speed pseudo-3D racer with traffic, crashes, laps, and a durable high score. */
class Formula1Game final {
  public:
    /**
     * @brief Constructs the game around the shared display, controls, safe gate HAL, and score
     *        slot.
     * @param display Display used for all frames; must outlive the game.
     * @param controls Control panel sampled once per loop pass; must outlive the game.
     * @param gateOutputs Gate driver kept low and disabled while the game runs.
     * @param leaderboard Durable ranking store handed to the arcade shell.
     */
    Formula1Game(
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
    friend struct Formula1GameTestAccess;
    /** @brief One traffic vehicle approaching from the horizon. */
    struct Car final {
        std::int8_t lane = 0;  ///< Lane -1, 0, or +1.
        std::int16_t y = -20;  ///< Top screen row, pixels.
        bool active = false;   ///< Slot holds a vehicle.
    };

    /** @brief Restores player, speed, traffic, crashes, and score state for a new run. */
    void resetSession();
    /**
     * @brief Advances steering, automatic speed, traffic, crash recovery, and score state.
     *
     * Steering is taken on every call outside a crash; physics advances at most once per 45 ms.
     * A crash lasts 820 ms; the last allowed crash ends the run.
     * @param controls Control sample of this loop pass.
     * @param nowMs Current system time in milliseconds.
     */
    void update(const hal::ControlSample& controls, std::uint32_t nowMs);
    /** @brief Renders the complete current racer frame to the 128x64 OLED. */
    void render();
    /** @brief Activates one deterministic pseudo-random traffic vehicle. */
    void spawnTraffic();
    /**
     * @brief Enters the visible collision animation and consumes one crash allowance.
     *
     * Stops the car and deducts up to 100 points.
     * @param nowMs Current system time in milliseconds; start of the crash animation.
     */
    void startCrash(std::uint32_t nowMs);
    /** @brief Routes the completed score into the shared arcade ranking flow. */
    void finishScore();
    /**
     * @brief Advances the local deterministic PRNG used only for game traffic.
     * @return Next xorshift32 state.
     */
    std::uint32_t nextRandom();

    /** Borrowed display. */
    hal::OledDisplay& display_;
    /** Borrowed control panel. */
    hal::ControlPanel& controls_;
    /** Borrowed gate driver, held low while the game runs. */
    hal::GateOutputDriver& gateOutputs_;
    /** Title, ranking, and game-over flow shared by the arcade titles. */
    ArcadeShell shell_;
    /** Best score shown in the HUD. */
    LeaderboardEntry highScore_{};
    /** Traffic vehicle slots. */
    std::array<Car, 4U> traffic_{};
    /** Centre column of the player car, pixels. */
    std::int16_t playerX_ = 61;
    /** Displayed speed in game units (not a physical unit); sets distance and score gain. */
    std::uint16_t speed_ = 0U;
    /** Track position in track units; drives curves, speed phases, and laps. */
    std::uint32_t distance_ = 0U;
    /** Race time of the run, ms; shown in seconds. */
    std::uint32_t elapsedMs_ = 0U;
    /** Score of the current run, capped at 999999. */
    std::uint32_t score_ = 0U;
    /** Displayed lap, 1..9, derived from distance_. */
    std::uint8_t lap_ = 1U;
    /** Crashes left before the run ends. */
    std::uint8_t crashesRemaining_ = 3U;
    /** xorshift32 state; not reset per session. */
    std::uint32_t randomState_ = 0xC064F1A5U;
    /** Time of the last physics frame, ms. */
    std::uint32_t lastPhysicsAtMs_ = 0U;
    /** True after the first physics frame of a run; lastPhysicsAtMs_ may be any value. */
    bool physicsStarted_ = false;
    /** Time of the last traffic spawn, ms. */
    std::uint32_t lastSpawnAtMs_ = 0U;
    /** Encoder-button long press that exits the game. */
    LongPressDetector exitHold_{};
    /** Start time of the current crash, ms. */
    std::uint32_t crashStartedAtMs_ = 0U;
#ifdef CLOCK_SIMULATOR
    /** Time of the last simulator frame, ms. */
    std::uint32_t lastSimulatorFrameAtMs_ = 0U;
#endif
    /** Crash animation is running; input is ignored. */
    bool crashed_ = false;
    /** All crashes are used; the score is final. */
    bool gameOver_ = false;
    /** Score was handed to the shell once. */
    bool scoreFinalized_ = false;
    /** Long press requested exit from run(). */
    bool exitRequested_ = false;
};

}  // namespace clockfw::game
