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

    /** @brief Converts the wrapping quadrature transition counter into complete detents. */
    std::int8_t sampleEncoder();

    /**
     * @brief Re-anchors quadrature bookkeeping after the encoder push switch settles.
     *
     * Axial push/release of the PEC11R shaft and contact bounce can
     * leave a harmless partial quadrature residue even though the shaft is again
     * sitting in a mechanical detent. Re-anchoring after a debounced release
     * prevents that residue from consuming the next detent or direction reversal.
     */
    void resynchronizeEncoderAtRest();

    DebouncedButton encoderButton_;
    DebouncedButton transportButton_;
    DebouncedButton tapButton_;
    DebouncedButton resetButton_;
    std::uint32_t encoderLastTransitionCount_ = 0U;
    std::uint8_t encoderDetentPhase_ = 0U;
    std::int8_t encoderTransitionRemainder_ = 0;
    std::int16_t pendingEncoderDetents_ = 0;
    bool encoderDirectionReversed_ = false;
};

}  // namespace clockfw::hal
