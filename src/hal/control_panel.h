/**
 * @file control_panel.h
 * @brief HAL driver for the rotary encoder and three front-panel buttons.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

namespace clockfw::hal {

/** @brief Edge event emitted by one debounced push button. */
enum class ButtonEdge : std::uint8_t { None, Pressed, Released };

/**
 * @brief Debounced state and edge event for one physical button.
 *
 * Timestamps are TIM5 microseconds of the first raw edge the control loop saw for
 * the accepted transition (F-029); debouncing only decides acceptance. Every value
 * of the wrapping 32-bit time base is valid, including 0, so validity is carried
 * separately (F-073).
 */
struct ButtonSample {
    ButtonEdge edge = ButtonEdge::None;
    bool pressed = false;
    /** First raw edge of the transition that this validated edge accepts. */
    std::uint32_t edgeTimestampUs = 0U;
    /** True when edge is not None and edgeTimestampUs holds its first raw edge. */
    bool edgeTimestampValid = false;
    /** First raw edge of the press that is held or released in this sample. */
    std::uint32_t pressTimestampUs = 0U;
    /** True while a press is held and on the sample that reports its release. */
    bool pressTimestampValid = false;
};

/** @brief Complete control-panel sample produced once per application loop. */
struct ControlSample {
    std::int8_t encoderDelta = 0;
    ButtonSample encoderButton{};
    ButtonSample transportButton{};
    ButtonSample tapButton{};
    ButtonSample resetButton{};
};

/** @brief Reads, debounces, and decodes all physical front-panel controls. */
class ControlPanel final {
  public:
    /** @brief Constructs the control panel with the board-specific GPIO assignment. */
    ControlPanel();

    /** @brief Configures GPIO inputs and captures the initial encoder/button state. */
    void begin();

    /**
     * @brief Samples all controls and emits debounced edge events.
     * @param nowMs Current monotonic time in milliseconds.
     * @return Current encoder delta and button states.
     */
    ControlSample sample(std::uint32_t nowMs);

    /** @brief Reverses the semantic sign of completed encoder detents without touching quadrature decoding. */
    void setEncoderDirectionReversed(bool reversed);

#if defined(CLOCK_HOST_TEST)
    /** @brief Encoder anchor diagnostics, the host-test copy of the W-070 debug counters. */
    struct EncoderDiagnosticsForTest {
        std::uint32_t anchors = 0U;
        std::uint8_t anchorRequestPhase = 0U;
        std::uint32_t discardedRemainders = 0U;
    };

    /** @brief Returns the encoder anchor diagnostics (host tests only). */
    EncoderDiagnosticsForTest encoderDiagnosticsForTest() const {
        return encoderDiagnostics_;
    }
#endif

  private:
    /** @brief Small state machine used for one active-low button input. */
    class DebouncedButton final {
      public:
        /** @brief Associates the helper with a physical GPIO pin. */
        explicit DebouncedButton(std::uint32_t pin);

        /** @brief Initializes the active-low input with the MCU pull-up enabled. */
        void begin(std::uint32_t nowMs);

        /**
         * @brief Samples and debounces the physical button.
         * @param nowMs Current monotonic time in milliseconds.
         * @return Debounced edge and stable pressed state.
         */
        ButtonSample sample(std::uint32_t nowMs);

      private:
        std::uint32_t pin_;
        bool rawPressed_ = false;
        bool stablePressed_ = false;
        /** Last raw change (ms); the debounce interval runs from here. */
        std::uint32_t rawChangedAtMs_ = 0U;
        /** True from the first raw edge away from the stable state until it settles. */
        bool transitionPending_ = false;
        /** TIM5 microseconds of that first raw edge. */
        std::uint32_t transitionStartedAtUs_ = 0U;
        /** First raw edge of the accepted press, valid while it is held. */
        std::uint32_t pressStartedAtUs_ = 0U;
        /** False for a press already held at begin(): its first edge was not observed. */
        bool pressStartKnown_ = false;
    };

    /**
     * @brief Converts the wrapping quadrature transition counter into complete detents.
     * @param nowMs Current monotonic time in milliseconds; marks quadrature activity.
     */
    std::int8_t sampleEncoder(std::uint32_t nowMs);

    /**
     * @brief Requests an anchor after the encoder push switch was released (F-010).
     *
     * An axial push or release of the PEC11R shaft and contact bounce can leave a
     * partial transition count although the shaft is back in a detent. The anchor
     * drops it; anchorEncoderAtRest() takes it once the encoder rests in a detent.
     */
    void requestEncoderAnchor();

    /**
     * @brief Takes a requested anchor once A/B rest in the detent state (F-010).
     * @param nowMs Current monotonic time in milliseconds.
     *
     * Requires the datasheet detent state and no quadrature transition for
     * kEncoderAnchorQuietMs; until then the request stays pending. The anchor
     * discards the partial transition count; completed and pending detents stay.
     */
    void anchorEncoderAtRest(std::uint32_t nowMs);

    /** @brief Counts one discarded partial transition count (W-070 diagnostics). */
    void countDiscardedEncoderRemainder();

    DebouncedButton encoderButton_;
    DebouncedButton transportButton_;
    DebouncedButton tapButton_;
    DebouncedButton resetButton_;
    std::uint32_t encoderLastTransitionCount_ = 0U;
    /** Sample time (ms) of the last observed quadrature transition. */
    std::uint32_t encoderLastActivityMs_ = 0U;
    /** True from an encoder-button release until the anchor is taken. */
    bool encoderAnchorPending_ = false;
    std::int8_t encoderTransitionRemainder_ = 0;
    std::int16_t pendingEncoderDetents_ = 0;
    bool encoderDirectionReversed_ = false;
#if defined(CLOCK_HOST_TEST)
    EncoderDiagnosticsForTest encoderDiagnostics_{};
#endif
};

}  // namespace clockfw::hal
