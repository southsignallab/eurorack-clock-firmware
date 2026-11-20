/**
 * @file groove_recorder.h
 * @brief Deterministic Custom Groove capture state machine driven by the ClockEngine timeline.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstdint>

#include "domain/custom_groove.h"

namespace clockfw::services {

/**
 * @brief Recording pass behavior.
 *
 * OneShot stops after one pattern length; Endless keeps cycling through the pattern
 * until stopped, and later TAPs overwrite earlier ones on the same step.
 */
enum class GrooveRecordMode : std::uint8_t { OneShot, Endless };
/**
 * @brief Recorder state machine phase.
 *
 * Ready: idle; PreCount: the Count-In runs; Recording: TAPs are captured.
 */
enum class GrooveRecordState : std::uint8_t { Ready, PreCount, Recording };

/** @brief Read-only recorder state used by the UI renderer. */
struct GrooveRecorderView final {
    /** One-Shot or Endless behavior. */
    GrooveRecordMode mode = GrooveRecordMode::OneShot;
    /** Current state machine phase. */
    GrooveRecordState state = GrooveRecordState::Ready;
    /** Configured Count-In length in master beats (0..64). */
    std::uint8_t countInBeats = 4U;
    /** Master beats left in the running Count-In, rounded up. */
    std::uint8_t countInRemaining = 0U;
    /** Current playhead step (0..length-1). */
    std::uint8_t playheadStep = 0U;
    /** Position inside the current step in 1/256 steps (0..255). */
    std::uint8_t playheadPhase256 = 0U;
    /** Captured steps, bit n = step n. */
    std::uint64_t capturedMask = 0U;
};

/**
 * @brief Captures human TAP timing directly into the existing signed Custom Groove format.
 *
 * The recorder works entirely in ClockEngine Q32 beat-space. Tempo changes therefore
 * change wall-clock speed without changing the pattern phase or capture math.
 */
class GrooveRecorder final {
  public:
    /**
     * @brief Returns the recorder to READY with the supplied non-musical capture settings.
     * @param mode One-Shot or Endless behavior.
     * @param countInBeats Count-In length in master beats; clamped to 64.
     * @param capturedMask Initial captured-step bitmask (bit n = step n).
     */
    void reset(GrooveRecordMode mode, std::uint8_t countInBeats, std::uint64_t capturedMask = 0U);

    /**
     * @brief Changes One-Shot/Endless behavior for the next/current recording pass.
     * @param mode New behavior.
     */
    void setMode(GrooveRecordMode mode);

    /**
     * @brief Changes the recorder-local Count-In length in master beats.
     * @param beats Count-In length in master beats; clamped to 64. Updates the remaining
     *              count only while READY.
     */
    void setCountInBeats(std::uint8_t beats);

    /**
     * @brief Arms recording on a grid that starts where the recording starts.
     *
     * Same as the four-argument start() with the grid origin at the end of the Count-In.
     * @param masterPositionQ32 Engine position (Q32 beats) when recording is armed.
     * @param stepIntervalQ32 Nominal step interval (Q32 beats).
     * @param length Pattern length in steps.
     */
    void start(std::uint64_t masterPositionQ32, std::uint64_t stepIntervalQ32, std::uint8_t length);

    /**
     * @brief Arms recording on the grid of the channel that plays the groove (F-022).
     * @param masterPositionQ32 Engine position (Q32 beats) when recording is armed.
     * @param gridOriginQ32 Position (Q32 beats) of event serial 0 of the channel (epoch plus
     *                      phase).
     * @param stepIntervalQ32 Nominal event interval of the channel (Q32 beats).
     * @param length Pattern length in steps; 0, a length above kCustomGrooveMaximumSteps or a
     *               zero interval stops the recorder instead.
     *
     * After the Count-In, capture starts at the next grid step. Steps are numbered as
     * playback numbers them, event serial modulo @p length, so a recording started in the
     * middle of a step plays back where it was tapped.
     */
    void start(
        std::uint64_t masterPositionQ32,
        std::uint64_t gridOriginQ32,
        std::uint64_t stepIntervalQ32,
        std::uint8_t length);

    /** @brief Stops recording and returns the playhead to the start without changing the draft. */
    void stop();

    /**
     * @brief Advances Pre-Count/playhead state from the current engine position.
     * @param masterPositionQ32 Current engine position (Q32 beats).
     * @param length Pattern length in steps.
     * @return True when the state, playhead, phase or remaining Count-In changed.
     */
    bool service(std::uint64_t masterPositionQ32, std::uint8_t length);

    /**
     * @brief Records one TAP into the nearest nominal step of the current cycle.
     * @param tapPositionQ32 Engine position (Q32 beats) of the TAP.
     * @param pattern Draft pattern; the offset of the captured step is overwritten.
     * @return True when the TAP was captured; false outside RECORDING, for an invalid
     *         pattern, before the first grid step, or past the end of a One-Shot pass.
     */
    bool capture(std::uint64_t tapPositionQ32, CustomGroovePattern& pattern);

    /** @brief Clears captured-state bookkeeping; the caller owns actual pattern clearing. */
    void clearCaptured();

    /**
     * @brief Returns the compact immutable state consumed by the UI.
     * @return Copy of the current recorder state.
     */
    GrooveRecorderView view() const;

  private:
    /** One-Shot or Endless behavior. */
    GrooveRecordMode mode_ = GrooveRecordMode::OneShot;
    /** Current state machine phase. */
    GrooveRecordState state_ = GrooveRecordState::Ready;
    /** Configured Count-In length in master beats (0..64). */
    std::uint8_t countInBeats_ = 4U;
    /** Master beats left in the running Count-In, rounded up. */
    std::uint8_t countInRemaining_ = 0U;
    /** Current playhead step (0..length-1). */
    std::uint8_t playheadStep_ = 0U;
    /** Position inside the current step in 1/256 steps (0..255). */
    std::uint8_t playheadPhase256_ = 0U;
    /** Captured steps, bit n = step n. */
    std::uint64_t capturedMask_ = 0U;
    /** Nominal step interval (Q32 beats); 0 when not armed. */
    std::uint64_t stepIntervalQ32_ = 0U;
    /** Engine position (Q32 beats) where the Count-In started. */
    std::uint64_t preCountEpochQ32_ = 0U;
    /** Position (Q32 beats) of the first recorded grid step. */
    std::uint64_t recordEpochQ32_ = 0U;
    /** Position (Q32 beats) where the Count-In ends. */
    std::uint64_t recordStartTargetQ32_ = 0U;
    /** Playback step of the first recorded grid step: its event serial modulo the length. */
    std::uint8_t stepBase_ = 0U;
};

}  // namespace clockfw::services
