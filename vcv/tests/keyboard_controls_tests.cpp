/**
 * @file keyboard_controls_tests.cpp
 * @brief Host tests for the Rack-independent VCV keyboard-to-panel mapping.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>

#include "keyboard_controls.h"

namespace {

using clockfw::vcv::applyKeyboardEvent;
using clockfw::vcv::KeyAction;
using clockfw::vcv::KeyboardEventResult;
using clockfw::vcv::KeyboardHeldControls;
using clockfw::vcv::PanelKey;
using clockfw::vcv::reconcileHeldControls;
using clockfw::vcv::releaseHeldControls;

/** @brief Fails the test binary independently of NDEBUG. */
void require(const bool condition, const char* const message) {
    if (!condition) {
        std::cerr << "keyboard-controls test failed: " << message << '\n';
        std::exit(1);
    }
}

/** @brief Returns the held flag that one hold-type key drives. */
bool heldFlag(const KeyboardHeldControls& held, const PanelKey key) {
    switch (key) {
        case PanelKey::LeftShift:
            return held.leftShift;
        case PanelKey::RightShift:
            return held.rightShift;
        case PanelKey::EncoderPush:
            return held.encoder;
        case PanelKey::Play:
            return held.play;
        case PanelKey::Tap:
            return held.tap;
        case PanelKey::Stop:
            return held.stop;
        default:
            return false;
    }
}

bool noneHeld(const KeyboardHeldControls& held) {
    return !held.leftShift && !held.rightShift && !held.encoder && !held.play && !held.tap &&
        !held.stop;
}

enum class Step : std::uint8_t { KeyDown, ModifierDown, KeyUp, ModifierUp };

/**
 * @brief Issue #13: every interleaving of a mapped key and Ctrl/Alt/Super.
 *
 * A key pressed without a modifier is a CLOCK control and must be released by its own key
 * release, whatever the modifier does in between. A key pressed while a modifier is held is
 * a Rack shortcut and must never become a held CLOCK control.
 */
void testModifierInterleavingsNeverLeaveControlsHeld() {
    constexpr std::array<std::array<Step, 4U>, 6U> kOrders{{
        {{Step::KeyDown, Step::KeyUp, Step::ModifierDown, Step::ModifierUp}},
        {{Step::KeyDown, Step::ModifierDown, Step::KeyUp, Step::ModifierUp}},
        {{Step::KeyDown, Step::ModifierDown, Step::ModifierUp, Step::KeyUp}},
        {{Step::ModifierDown, Step::KeyDown, Step::KeyUp, Step::ModifierUp}},
        {{Step::ModifierDown, Step::KeyDown, Step::ModifierUp, Step::KeyUp}},
        {{Step::ModifierDown, Step::ModifierUp, Step::KeyDown, Step::KeyUp}},
    }};
    constexpr std::array<PanelKey, 6U> kHoldKeys{
        {PanelKey::EncoderPush,
         PanelKey::Play,
         PanelKey::Tap,
         PanelKey::Stop,
         PanelKey::LeftShift,
         PanelKey::RightShift}};

    for (const PanelKey key : kHoldKeys) {
        const bool isShift = key == PanelKey::LeftShift || key == PanelKey::RightShift;
        for (const auto& order : kOrders) {
            KeyboardHeldControls held{};
            bool modifierDown = false;
            bool pressedAsControl = false;
            for (const Step step : order) {
                switch (step) {
                    case Step::ModifierDown:
                        modifierDown = true;
                        // The modifier key itself is not a CLOCK key and goes to Rack.
                        require(
                            !applyKeyboardEvent(held, PanelKey::Unmapped, KeyAction::Press, true)
                                 .consumed,
                            "modifier press must reach Rack");
                        break;
                    case Step::ModifierUp:
                        modifierDown = false;
                        require(
                            !applyKeyboardEvent(held, PanelKey::Unmapped, KeyAction::Release, false)
                                 .consumed,
                            "modifier release must reach Rack");
                        break;
                    case Step::KeyDown: {
                        const KeyboardEventResult result =
                            applyKeyboardEvent(held, key, KeyAction::Press, modifierDown);
                        pressedAsControl = isShift || !modifierDown;
                        require(
                            result.consumed == pressedAsControl,
                            "a modified press is a Rack shortcut, an unmodified one a CLOCK "
                            "control");
                        require(
                            heldFlag(held, key) == pressedAsControl,
                            "press must hold exactly the CLOCK controls it consumed");
                        // Auto-repeat while the modifier state changes must not alter the hold.
                        (void)applyKeyboardEvent(held, key, KeyAction::Repeat, modifierDown);
                        require(
                            heldFlag(held, key) == pressedAsControl, "repeat must keep the hold");
                        break;
                    }
                    case Step::KeyUp: {
                        const KeyboardEventResult result =
                            applyKeyboardEvent(held, key, KeyAction::Release, modifierDown);
                        require(!heldFlag(held, key), "key release must release its control");
                        if (pressedAsControl) {
                            require(result.consumed, "release of a held control belongs to CLOCK");
                        }
                        break;
                    }
                }
            }
            require(noneHeld(held), "no virtual control may stay held after all keys are up");
        }
    }
}

/** @brief Unchanged behavior: encoder detents, unmapped keys and modified encoder turns. */
void testEncoderKeysAndForwarding() {
    KeyboardHeldControls held{};
    KeyboardEventResult result =
        applyKeyboardEvent(held, PanelKey::EncoderUp, KeyAction::Press, false);
    require(result.consumed && result.encoderDetents == 1, "Up is one clockwise detent");
    result = applyKeyboardEvent(held, PanelKey::EncoderDown, KeyAction::Repeat, false);
    require(
        result.consumed && result.encoderDetents == -1,
        "Down repeat is one counter-clockwise detent");
    result = applyKeyboardEvent(held, PanelKey::EncoderUp, KeyAction::Release, false);
    require(result.consumed && result.encoderDetents == 0, "encoder key release queues nothing");
    result = applyKeyboardEvent(held, PanelKey::EncoderUp, KeyAction::Press, true);
    require(!result.consumed && result.encoderDetents == 0, "Ctrl+Up remains a Rack shortcut");
    result = applyKeyboardEvent(held, PanelKey::Unmapped, KeyAction::Press, false);
    require(!result.consumed, "unmapped keys reach Rack");
    // A release of a control that is not held, with a modifier, stays a Rack event.
    result = applyKeyboardEvent(held, PanelKey::Tap, KeyAction::Release, true);
    require(!result.consumed && noneHeld(held), "Ctrl+T release without a held TAP reaches Rack");
}

/** @brief F-065: the pointer leaves the module while keys hold controls. */
void testPointerLeavingReleasesEveryHeldControl() {
    KeyboardHeldControls held{};
    require(!releaseHeldControls(held), "nothing held: nothing to release");
    applyKeyboardEvent(held, PanelKey::Play, KeyAction::Press, false);
    applyKeyboardEvent(held, PanelKey::Tap, KeyAction::Press, false);
    applyKeyboardEvent(held, PanelKey::LeftShift, KeyAction::Press, false);
    applyKeyboardEvent(held, PanelKey::EncoderPush, KeyAction::Press, false);
    applyKeyboardEvent(held, PanelKey::Stop, KeyAction::Press, false);
    applyKeyboardEvent(held, PanelKey::RightShift, KeyAction::Press, false);
    require(releaseHeldControls(held), "held controls are released when the pointer leaves");
    require(noneHeld(held), "no control stays held after the pointer left");
}

/** @brief F-065: a key release that reached another widget still ends its hold. */
void testReleaseElsewhereIsFoundFromTheKeyState() {
    KeyboardHeldControls held{};
    applyKeyboardEvent(held, PanelKey::Play, KeyAction::Press, false);
    applyKeyboardEvent(held, PanelKey::Tap, KeyAction::Press, false);
    const auto onlyTapDown = [](const PanelKey key) { return key == PanelKey::Tap; };
    require(reconcileHeldControls(held, onlyTapDown), "a key that is up releases its control");
    require(!held.play && held.tap, "only the control whose key is up is released");
    require(!reconcileHeldControls(held, onlyTapDown), "nothing changes while the key stays down");
    const auto nothingDown = [](PanelKey) { return false; };
    require(
        reconcileHeldControls(held, nothingDown) && noneHeld(held), "all keys up: nothing held");
    for (const PanelKey key :
         {PanelKey::LeftShift, PanelKey::RightShift, PanelKey::EncoderPush, PanelKey::Stop}) {
        applyKeyboardEvent(held, key, KeyAction::Press, false);
        require(
            reconcileHeldControls(held, nothingDown) && !heldFlag(held, key),
            "each held key is reconciled");
    }
}

}  // namespace

int main() {
    testModifierInterleavingsNeverLeaveControlsHeld();
    testEncoderKeysAndForwarding();
    testPointerLeavingReleasesEveryHeldControl();
    testReleaseElsewhereIsFoundFromTheKeyState();
    std::cout << "clock-vcv-keyboard-tests: PASS\n";
    return 0;
}
