/**
 * @file keyboard_controls.h
 * @brief Rack-independent mapping of host keyboard events onto CLOCK's virtual panel controls.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstdint>

namespace clockfw::vcv {

/** @brief Host keys that CLOCK maps onto a physical panel control. */
enum class PanelKey : std::uint8_t {
    Unmapped,     ///< Any key CLOCK does not use; always forwarded to Rack.
    LeftShift,    ///< Holds TAP / SHIFT.
    RightShift,   ///< Holds TAP / SHIFT.
    EncoderUp,    ///< One clockwise encoder detent per press or repeat.
    EncoderDown,  ///< One counter-clockwise encoder detent per press or repeat.
    EncoderPush,  ///< Holds the encoder push button.
    Play,         ///< Holds PLAY / PAUSE.
    Tap,          ///< Holds TAP.
    Stop,         ///< Holds STOP / BACK.
};

/** @brief One host key transition. */
enum class KeyAction : std::uint8_t {
    Press,    ///< Key went down.
    Repeat,   ///< Key auto-repeat while held.
    Release,  ///< Key went up.
};

/** @brief Virtual panel controls currently held through the host keyboard. */
struct KeyboardHeldControls final {
    bool leftShift = false;
    bool rightShift = false;
    bool encoder = false;
    bool play = false;
    bool tap = false;
    bool stop = false;
};

/** @brief Outcome of one host key event. */
struct KeyboardEventResult final {
    bool consumed = false;   ///< True when CLOCK handled the event; false forwards it to Rack.
    int encoderDetents = 0;  ///< Signed encoder detents to queue.
};

/**
 * @brief Applies one host key event to the keyboard-held virtual controls.
 *
 * Ctrl, Alt and Super belong to Rack's own shortcuts, so presses and repeats with one of
 * them held are forwarded to Rack. Shift stays available as the physical TAP / SHIFT hold.
 *
 * @param held Controls held through the keyboard; updated in place.
 * @param key Mapped host key.
 * @param action Press, repeat or release.
 * @param nonShiftModifiers True while Ctrl, Alt or Super is held.
 * @return Whether CLOCK consumed the event, and the encoder detents to queue.
 */
KeyboardEventResult applyKeyboardEvent(
    KeyboardHeldControls& held,
    PanelKey key,
    KeyAction action,
    bool nonShiftModifiers) noexcept;

}  // namespace clockfw::vcv
