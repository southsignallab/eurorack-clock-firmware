/**
 * @file beatknecht.h
 * @brief Boot-only eight-channel gate rhythm generator Easter egg.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstdint>

#include "domain/clock_types.h"
#include "game/arcade_shell.h"
#include "hal/control_panel.h"
#include "hal/gate_output_driver.h"
#include "hal/oled_display.h"

namespace clockfw::game {

/** @brief Eight-gate one-bar rhythm player with encoder tempo and TAP style selection. */
class Beatknecht final {
  public:
    /**
     * @brief Constructs the rhythm generator around the shared display, controls, and gate HAL.
     * @param display Display used for all frames; must outlive the generator.
     * @param controls Control panel sampled once per loop pass; must outlive the generator.
     * @param gateOutputs Gate driver; enabled only while playback runs, otherwise held low.
     */
    Beatknecht(
        hal::OledDisplay& display,
        hal::ControlPanel& controls,
        hal::GateOutputDriver& gateOutputs);
    /** @brief Runs the selected boot rhythm generator synchronously until exit. */
    void run();
#ifdef CLOCK_SIMULATOR
    /** @brief Starts a non-blocking simulator rhythm-generator session. */
    void beginForSimulator();
    /**
     * @brief Services one simulator iteration and returns true while the mode remains active.
     * @param nowMs Current system time in milliseconds.
     * @return true while no exit was confirmed.
     */
    bool serviceForSimulator(std::uint32_t nowMs);
#endif

  private:
    friend struct BeatknechtTestAccess;
    /**
     * @brief Restores the default style, tempo, step, and output state.
     *
     * Leaves the transport stopped; does not write the gate outputs.
     * @param nowMs Current system time in milliseconds; becomes the next step time.
     */
    void resetSession(std::uint32_t nowMs);
    /**
     * @brief Applies controls and advances gate timing.
     *
     * RESET stops, TRANSPORT toggles play and pause, TAP selects the next style, the encoder sets
     * the tempo (40..240 BPM). While playing, ends the gate pulse and triggers due steps; a step
     * more than one step duration late restarts the grid at nowMs.
     * @param controls Control sample of this loop pass.
     * @param nowMs Current system time in milliseconds.
     */
    void update(const hal::ControlSample& controls, std::uint32_t nowMs);
    /** @brief Renders all eight patterns and the current step position. */
    void render();
    /**
     * @brief Emits the active gates for one sixteenth-note pattern step.
     * @param nowMs Current time; the gates fall a fixed pulse length later.
     * @param stepAtMs Target time of this step; the next step is scheduled from it (F-041).
     */
    void triggerStep(std::uint32_t nowMs, std::uint32_t stepAtMs);
    /**
     * @brief Starts from STOP or resumes a paused rhythm without resetting tempo/style.
     *
     * Enables the output stage. A start from STOP triggers step one at once.
     * @param nowMs Current system time in milliseconds.
     */
    void startOrResume(std::uint32_t nowMs);
    /**
     * @brief Pauses rhythm timing, preserves the remaining step interval, and forces all gates
     *        LOW.
     *
     * Does nothing unless playing.
     * @param nowMs Current system time in milliseconds.
     */
    void pause(std::uint32_t nowMs);
    /**
     * @brief Stops playback, resets the pattern phase to step one, and mutes all gate source
     *        outputs.
     */
    void stop();
    /** @brief Drives all eight gate sources low immediately. */
    void stopOutputs();
    /**
     * @brief Opens the guarded exit confirmation and temporarily silences active playback.
     *
     * Playing transport is paused; otherwise the gates are driven low.
     * @param nowMs Current system time in milliseconds.
     */
    void beginExitConfirmation(std::uint32_t nowMs);
    /**
     * @brief Services NO/YES selection while the exit confirmation is active.
     *
     * Waits for the long press to be released first. YES stops playback and requests exit; NO
     * resumes playback if it was playing before the dialog opened.
     * @param controls Control sample of this loop pass.
     * @param nowMs Current system time in milliseconds.
     */
    void updateExitConfirmation(const hal::ControlSample& controls, std::uint32_t nowMs);
    /** @brief Draws the shared arcade-style exit confirmation dialog. */
    void renderExitConfirmation();
    /**
     * @brief Returns the current sixteenth-note duration in milliseconds.
     * @return 15000 / bpm_, truncated; triggerStep() carries the remainder.
     */
    std::uint32_t stepDurationMs() const;

    /** Borrowed display. */
    hal::OledDisplay& display_;
    /** Borrowed control panel. */
    hal::ControlPanel& controls_;
    /** Borrowed gate driver; drives the eight drum gates. */
    hal::GateOutputDriver& gateOutputs_;
    /** Intro screen; constructed without a leaderboard. */
    ArcadeShell shell_;
    /** Tempo in beats per minute, 40..240. */
    std::uint16_t bpm_ = 120U;
    /** Selected drum style, index into the style table. */
    std::uint8_t styleIndex_ = 0U;
    /** Pattern step triggered next, 0..15. */
    std::uint8_t nextStep_ = 0U;
    /** Pattern step shown as current, 0..15. */
    std::uint8_t displayStep_ = 0U;
    /** Target time of the next step, ms. */
    std::uint32_t nextStepAtMs_ = 0U;
    /** Accumulated remainder of 15000 / bpm_ in units of 1 / bpm_ ms (F-041). */
    std::uint32_t stepRemainder_ = 0U;
    /** Time the current 12 ms gate pulse ends, ms. */
    std::uint32_t gatesOffAtMs_ = 0U;
    /** Encoder-button long press that opens the exit dialog. */
    LongPressDetector exitHold_{};
    /** Time left to the next step when paused, ms. */
    std::uint32_t pausedStepRemainingMs_ = 0U;
    /** Playback state. */
    TransportState transport_ = TransportState::Stopped;
    /** Playback state when the exit dialog opened; restored on NO. */
    TransportState transportBeforeExitConfirmation_ = TransportState::Stopped;
#ifdef CLOCK_SIMULATOR
    /** Time of the last simulator frame, ms. */
    std::uint32_t lastSimulatorFrameAtMs_ = 0U;
#endif
    /** At least one gate of the current step is high. */
    bool gatesHigh_ = false;
    /** Exit was confirmed. */
    bool exitRequested_ = false;
    /** The exit dialog owns the controls and display. */
    bool exitConfirmationActive_ = false;
    /** YES is selected in the exit dialog. */
    bool exitYesSelected_ = false;
    /** The exit dialog waits for the encoder button to be released. */
    bool exitWaitForRelease_ = false;
};

}  // namespace clockfw::game
