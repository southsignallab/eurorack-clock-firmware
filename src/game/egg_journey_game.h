/**
 * @file egg_journey_game.h
 * @brief Boot-only monochrome lunar traversal Easter egg starring a fragile egg.
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

/**
 * @brief Lunar obstacle game with craters, falling asteroids, jumping, and a durable high
 *        score.
 */
namespace egg_journey {

/** Screen row of the ground line, in pixels. */
inline constexpr std::int16_t kGroundY = 46;
/** Height of the egg sprite, in pixels. */
inline constexpr std::int16_t kEggHeight = 11;
/** Fixed-point scale of the jump physics: units per pixel. */
inline constexpr std::int16_t kPhysicsScale = 8;

}  // namespace egg_journey

/**
 * @brief Boot-only lunar traversal game: steer an egg over craters and dodge falling asteroids.
 *
 * Owns no hardware; it borrows the display, control panel, and gate driver for the duration of
 * run(). Gate outputs are forced low and the output stage disabled while the game runs.
 */
class EggJourneyGame final {
  public:
    /**
     * @brief Constructs the game around the shared display, controls, safe gate HAL, and score
     *        slot.
     * @param display Display used for all frames; must outlive the game.
     * @param controls Control panel sampled once per loop pass; must outlive the game.
     * @param gateOutputs Gate driver kept low and disabled while the game runs.
     * @param leaderboard Durable ranking store handed to the arcade shell.
     */
    EggJourneyGame(
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
    friend struct EggJourneyGameTestAccess;

    /** @brief Visible reason for the most recent lost life. */
    enum class FailureMode : std::uint8_t {
        None,      ///< No failure is shown.
        Broken,    ///< The egg fell into a crater.
        Flattened  ///< An asteroid landed on the egg.
    };

    /** @brief Crater left by an asteroid impact during the current session. */
    struct ImpactCrater final {
        std::int32_t worldX = 0;  ///< Crater centre in world coordinates, pixels.
        std::int16_t radius = 0;  ///< Crater half-width, pixels.
        bool active = false;      ///< Slot holds a crater.
    };

    /** @brief The single falling asteroid; at most one is in flight. */
    struct Asteroid final {
        std::int32_t targetWorldX = 0;  ///< Landing position in world coordinates, pixels.
        std::int16_t y = 0;             ///< Current screen row, pixels from the top.
        bool active = false;            ///< Asteroid is falling.
    };

    /** @brief Restores player, terrain-event, and score state for a fresh run. */
    void resetSession();
    /**
     * @brief Advances horizontal movement, jump physics, crater collisions, and asteroid state.
     *
     * Input is taken on every call; physics advances at most once per 45 ms frame interval.
     * @param controls Control sample of this loop pass.
     * @param nowMs Current system time in milliseconds.
     */
    void update(const hal::ControlSample& controls, std::uint32_t nowMs);
    /** @brief Renders the current lunar landscape and player state. */
    void render();
    /**
     * @brief Advances the deterministic local PRNG used for terrain events.
     * @return Next xorshift32 state.
     */
    std::uint32_t nextRandom();
    /**
     * @brief Starts one telegraphed asteroid fall at an avoidable offset from the player.
     *
     * Does nothing while an asteroid is falling or the game is over; also picks the random delay
     * until the next asteroid.
     * @param nowMs Current system time in milliseconds, stored as the spawn time.
     */
    void spawnAsteroid(std::uint32_t nowMs);
    /**
     * @brief Resolves asteroid impact and leaves a persistent crater in the current session.
     *
     * Costs a life (FailureMode::Flattened) when the player is within 7 pixels of the impact,
     * otherwise awards the dodge bonus.
     * @param nowMs Current system time in milliseconds; starts the 320 ms impact flash.
     */
    void impactAsteroid(std::uint32_t nowMs);
    /**
     * @brief Adds one dynamic impact crater, recycling the oldest slot when necessary.
     * @param worldX Crater centre in world coordinates, pixels.
     * @param radius Crater half-width, pixels.
     */
    void addImpactCrater(std::int32_t worldX, std::int16_t radius);
    /**
     * @brief Returns crater depression in pixels at the supplied world coordinate.
     * @param worldX World coordinate, pixels.
     * @return Deepest static or impact crater depth at worldX, 0 to 6 pixels.
     */
    std::int16_t craterDepthAt(std::int32_t worldX) const;
    /**
     * @brief Consumes one life and enters the requested visible failure state.
     *
     * Ignored while a retry is pending or the game is over. The last life ends the game and
     * finalizes the score.
     * @param mode Failure to display.
     */
    void fail(FailureMode mode);
    /** @brief Respawns the egg safely after a non-terminal failure. */
    void respawnAfterFailure();
    /** @brief Routes the completed score into the shared arcade ranking flow. */
    void finishScore();

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
    /** Ring of asteroid craters of the current session. */
    std::array<ImpactCrater, 6U> impactCraters_{};
    /** The falling asteroid, if any. */
    Asteroid asteroid_{};
    /** Next impactCraters_ slot to overwrite. */
    std::size_t nextImpactCraterSlot_ = 0U;
    /** Egg position in world coordinates, pixels. */
    std::int32_t playerWorldX_ = 0;
    /** World coordinate of the left screen edge, pixels; scrolls automatically. */
    std::int32_t cameraWorldX_ = 0;
    /** Egg column on screen, pixels (clamped to 20..104). */
    std::int16_t playerScreenX_ = 42;
    /** Largest camera position reached, pixels; distance score accrues beyond it. */
    std::int32_t furthestDistance_ = 0;
    /** Horizontal speed in pixels per physics frame, -7..7, decays by 1 per frame. */
    std::int16_t horizontalVelocity_ = 0;
    /** Height above the ground in egg_journey::kPhysicsScale units per pixel. */
    std::int16_t jumpHeightFp_ = 0;
    /** Vertical speed in egg_journey::kPhysicsScale units per physics frame. */
    std::int16_t jumpVelocityFp_ = 0;
    /** Score of the current run, capped at 999999. */
    std::uint32_t score_ = 0U;
    /** xorshift32 state; reset to the same seed each session. */
    std::uint32_t randomState_ = 0x4D4F4F4EU;
    /** Time of the last physics frame, ms. */
    std::uint32_t lastPhysicsAtMs_ = 0U;
    /** Time of the last asteroid spawn, ms. */
    std::uint32_t lastAsteroidAtMs_ = 0U;
    /** Delay from lastAsteroidAtMs_ to the next spawn, ms. */
    std::uint32_t nextAsteroidDelayMs_ = 5200U;
    /** End of the impact flash, ms; compared with lastPhysicsAtMs_. */
    std::uint32_t impactFlashUntilMs_ = 0U;
    /** Encoder-button long press that exits the game. */
    LongPressDetector exitHold_{};
    /** Remaining lives, 0..3. */
    std::uint8_t lives_ = 3U;
#ifdef CLOCK_SIMULATOR
    /** Time of the last simulator frame, ms. */
    std::uint32_t lastSimulatorFrameAtMs_ = 0U;
#endif
    /** Failure currently displayed. */
    FailureMode failureMode_ = FailureMode::None;
    /** All lives are used; the score is final. */
    bool gameOver_ = false;
    /** A life was lost; a fresh TAP press respawns the egg. */
    bool awaitingRetry_ = false;
    /** TAP was released since the failure, so the next press counts. */
    bool retryArmed_ = false;
    /** Score was handed to the shell once. */
    bool scoreFinalized_ = false;
    /** Long press requested exit from run(). */
    bool exitRequested_ = false;
};

}  // namespace clockfw::game
