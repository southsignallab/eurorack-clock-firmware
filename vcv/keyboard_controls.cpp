/**
 * @file keyboard_controls.cpp
 * @brief Rack-independent mapping of host keyboard events onto CLOCK's virtual panel controls.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "keyboard_controls.h"

namespace clockfw::vcv {
namespace {

/** @brief Returns the held flag driven by one hold-type key, or nullptr for other keys. */
bool* heldControlFor(KeyboardHeldControls& held, const PanelKey key) noexcept {
    switch (key) {
        case PanelKey::LeftShift:
            return &held.leftShift;
        case PanelKey::RightShift:
            return &held.rightShift;
        case PanelKey::EncoderPush:
            return &held.encoder;
        case PanelKey::Play:
            return &held.play;
        case PanelKey::Tap:
            return &held.tap;
        case PanelKey::Stop:
            return &held.stop;
        case PanelKey::Unmapped:
        case PanelKey::EncoderUp:
        case PanelKey::EncoderDown:
            break;
    }
    return nullptr;
}

}  // namespace

KeyboardEventResult applyKeyboardEvent(
    KeyboardHeldControls& held,
    const PanelKey key,
    const KeyAction action,
    const bool nonShiftModifiers) noexcept {
    KeyboardEventResult result{};
    const bool pressed = action == KeyAction::Press || action == KeyAction::Repeat;

    // Shift is CLOCK's virtual second hand: while held, it is the physical TAP/SHIFT button.
    if (key == PanelKey::LeftShift || key == PanelKey::RightShift) {
        *heldControlFor(held, key) = pressed;
        result.consumed = true;
        return result;
    }

    // A release always ends a hold CLOCK accepted earlier, even when Ctrl/Alt/Super went
    // down in between; otherwise the virtual control would stay pressed (#13).
    bool* const heldControl = heldControlFor(held, key);
    if (action == KeyAction::Release && heldControl != nullptr && *heldControl) {
        *heldControl = false;
        result.consumed = true;
        return result;
    }

    // Preserve Rack's own Ctrl/Alt/Super shortcuts.
    if (nonShiftModifiers || key == PanelKey::Unmapped) {
        return result;
    }

    if (key == PanelKey::EncoderUp || key == PanelKey::EncoderDown) {
        if (pressed) {
            result.encoderDetents = key == PanelKey::EncoderUp ? 1 : -1;
        }
        result.consumed = true;
        return result;
    }

    *heldControl = pressed;
    result.consumed = true;
    return result;
}

}  // namespace clockfw::vcv
