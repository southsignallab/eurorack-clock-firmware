/**
 * @file story_decoder_action.cpp
 * @brief Storybook action decoding (one entry of a tutorial scene).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "tutorial/story_decoder.h"

#include "tutorial/story_focus_regions.h"
#include "tutorial/story_yaml.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
namespace clockfw::sim::tutorial::detail {

StoryAction StoryDecoder::decodeAction(
    const std::string& name,
    const Node& node,
    const std::size_t sceneIndex,
    const std::size_t actionIndex) {
    StoryAction action;
    action.sourceLine = node.line;
    if (name == "wait_ms") {
        action.kind = StoryActionKind::Wait;
        if (const auto value = uintValue(node, "wait_ms", 1U, 3600000U, sceneIndex, actionIndex))
            action.durationMs = *value;
        return action;
    }
    if (name == "beat_end") {
        action.kind = StoryActionKind::BeatEnd;
        if (const auto value = scalar(node, "beat_end", sceneIndex, actionIndex))
            action.narrationId = *value;
        return action;
    }
    if (!mapping(node, name.c_str(), sceneIndex, actionIndex))
        return action;
    if (name == "beat") {
        action.kind = StoryActionKind::BeatBegin;
        rejectUnknown(node, {"narration", "duration_ms"}, "beat", sceneIndex, actionIndex);
        if (const Node* value = required(node, "narration", "beat", sceneIndex, actionIndex)) {
            if (const auto parsed = scalar(*value, "beat.narration", sceneIndex, actionIndex))
                action.narrationId = *parsed;
        }
        if (const Node* value = required(node, "duration_ms", "beat", sceneIndex, actionIndex)) {
            if (const auto parsed =
                    uintValue(*value, "beat.duration_ms", 1U, 3600000U, sceneIndex, actionIndex))
                action.durationMs = *parsed;
        }
    } else if (name == "encoder") {
        action.kind = StoryActionKind::Encoder;
        rejectUnknown(node, {"direction", "detents", "speed"}, "encoder", sceneIndex, actionIndex);
        if (const Node* value = required(node, "direction", "encoder", sceneIndex, actionIndex)) {
            const auto text = scalar(*value, "encoder.direction", sceneIndex, actionIndex);
            if (text && *text == "clockwise")
                action.encoderDirection = 1;
            else if (text && (*text == "counter_clockwise" || *text == "counter-clockwise"))
                action.encoderDirection = -1;
            else if (text)
                issue(
                    *value, "encoder.direction: expected clockwise or counter_clockwise",
                    sceneIndex, actionIndex);
        }
        if (const Node* value = node.find("detents")) {
            if (const auto parsed =
                    uintValue(*value, "encoder.detents", 1U, 4096U, sceneIndex, actionIndex))
                action.detents = static_cast<std::uint16_t>(*parsed);
        }
        if (const Node* value = node.find("speed")) {
            const auto text = scalar(*value, "encoder.speed", sceneIndex, actionIndex);
            if (text && *text == "fast")
                action.encoderFast = true;
            else if (text && *text == "normal")
                action.encoderFast = false;
            else if (text)
                issue(*value, "encoder.speed: expected normal or fast", sceneIndex, actionIndex);
        }
    } else if (name == "encoder_push") {
        action.kind = StoryActionKind::EncoderPush;
        rejectUnknown(node, {"hold_ms"}, "encoder_push", sceneIndex, actionIndex);
        if (const Node* value = node.find("hold_ms")) {
            action.holdMs =
                uintValue(*value, "encoder_push.hold_ms", 1U, 10000U, sceneIndex, actionIndex);
        }
    } else if (name == "button") {
        action.kind = StoryActionKind::Button;
        rejectUnknown(node, {"name", "state"}, "button", sceneIndex, actionIndex);
        if (const Node* value = required(node, "name", "button", sceneIndex, actionIndex)) {
            const auto text = scalar(*value, "button.name", sceneIndex, actionIndex);
            if (text && *text == "PLAY")
                action.moduleControl = ModuleControl::Play;
            else if (text && *text == "TAP")
                action.moduleControl = ModuleControl::Tap;
            else if (text && (*text == "STOP" || *text == "STOP_BACK"))
                action.moduleControl = ModuleControl::StopBack;
            else if (text)
                issue(
                    *value, "button.name: unknown CLOCK button '" + *text + "'", sceneIndex,
                    actionIndex);
        }
        if (const Node* value = node.find("state")) {
            const auto text = scalar(*value, "button.state", sceneIndex, actionIndex);
            if (text && *text == "down")
                action.buttonState = true;
            else if (text && *text == "up")
                action.buttonState = false;
            else if (text)
                issue(*value, "button.state: expected 'down' or 'up'", sceneIndex, actionIndex);
        }
    } else if (name == "power" || name == "sync_cable" || name == "rst_cable" ||
               name == "sync_generator" || name == "rst_generator") {
        action.kind = name == "power"  ? StoryActionKind::Power
            : name == "sync_cable"     ? StoryActionKind::SyncCable
            : name == "rst_cable"      ? StoryActionKind::ResetCable
            : name == "sync_generator" ? StoryActionKind::SyncGenerator
                                       : StoryActionKind::ResetGenerator;
        rejectUnknown(node, {"state"}, name.c_str(), sceneIndex, actionIndex);
        if (const Node* value = required(node, "state", name.c_str(), sceneIndex, actionIndex)) {
            const auto text = scalar(*value, (name + ".state").c_str(), sceneIndex, actionIndex);
            if (text) {
                const bool onOff = name == "power";
                const bool cable = name == "sync_cable" || name == "rst_cable";
                const std::string yes = onOff ? "on" : cable ? "connect" : "run";
                const std::string no = onOff ? "off" : cable ? "disconnect" : "hold";
                if (*text == yes)
                    action.state = true;
                else if (*text == no)
                    action.state = false;
                else
                    issue(
                        *value, name + ".state: expected '" + yes + "' or '" + no + "'", sceneIndex,
                        actionIndex);
            }
        }
    } else if (name == "sync_source") {
        action.kind = StoryActionKind::SyncSource;
        rejectUnknown(node, {"bpm", "ppqn", "waveform"}, "sync_source", sceneIndex, actionIndex);
        if (const Node* value = node.find("bpm"))
            action.bpm = uintValue(*value, "sync_source.bpm", 1U, 999U, sceneIndex, actionIndex);
        if (const Node* value = node.find("ppqn")) {
            if (const auto parsed =
                    uintValue(*value, "sync_source.ppqn", 1U, 24U, sceneIndex, actionIndex))
                action.ppqn = static_cast<std::uint8_t>(*parsed);
        }
        if (const Node* value = node.find("waveform"))
            action.waveform = waveform(*value, sceneIndex, actionIndex);
        if (!action.bpm && !action.ppqn && !action.waveform)
            issue(node, "sync_source requires bpm, ppqn and/or waveform", sceneIndex, actionIndex);
        if (action.ppqn && *action.ppqn != 1U && *action.ppqn != 2U && *action.ppqn != 4U &&
            *action.ppqn != 24U)
            issue(node, "sync_source.ppqn must be 1, 2, 4 or 24", sceneIndex, actionIndex);
    } else if (name == "rst_pulse") {
        action.kind = StoryActionKind::ResetPulse;
        rejectUnknown(node, {}, "rst_pulse", sceneIndex, actionIndex);
    } else if (name == "subtitle") {
        action.kind = StoryActionKind::Subtitle;
        rejectUnknown(node, {"text"}, "subtitle", sceneIndex, actionIndex);
        if (const Node* value = required(node, "text", "subtitle", sceneIndex, actionIndex)) {
            if (const auto parsed = scalar(*value, "subtitle.text", sceneIndex, actionIndex))
                action.text = *parsed;
        }
    } else if (name == "scope") {
        action.kind = StoryActionKind::Scope;
        rejectUnknown(node, {"state", "channel"}, "scope", sceneIndex, actionIndex);
        if (const Node* value = required(node, "state", "scope", sceneIndex, actionIndex)) {
            const auto text = scalar(*value, "scope.state", sceneIndex, actionIndex);
            if (text && *text == "hide")
                action.scopeMode = ScopeMode::Hidden;
            else if (text && *text == "show")
                action.scopeMode = ScopeMode::VisibleChannel;
            else if (text)
                issue(*value, "scope.state: expected show or hide", sceneIndex, actionIndex);
        }
        if (const Node* value = node.find("channel")) {
            const auto text = scalar(*value, "scope.channel", sceneIndex, actionIndex);
            if (text && *text != "visible")
                issue(
                    *value, "scope.channel: Phase 1 supports only 'visible'", sceneIndex,
                    actionIndex);
        } else if (action.scopeMode == ScopeMode::VisibleChannel) {
            issue(node, "scope show requires channel: visible", sceneIndex, actionIndex);
        }
    } else if (name == "focus") {
        action.kind = StoryActionKind::Focus;
        rejectUnknown(
            node, {"target", "label", "placement", "duration_ms", "x", "y", "width", "height"},
            "focus", sceneIndex, actionIndex);
        if (const Node* value = required(node, "target", "focus", sceneIndex, actionIndex)) {
            const auto text = scalar(*value, "focus.target", sceneIndex, actionIndex);
            if (text && *text == "encoder")
                action.focusTarget = FocusTarget::Encoder;
            else if (text && *text == "play")
                action.focusTarget = FocusTarget::Play;
            else if (text && *text == "tap")
                action.focusTarget = FocusTarget::Tap;
            else if (text && (*text == "stop" || *text == "stop_back"))
                action.focusTarget = FocusTarget::StopBack;
            else if (text && *text == "sync")
                action.focusTarget = FocusTarget::Sync;
            else if (text && (*text == "rst" || *text == "reset"))
                action.focusTarget = FocusTarget::Reset;
            else if (text && *text == "oled_region")
                action.focusTarget = FocusTarget::OledRegion;
            else if (text) {
                if (const auto region = storyOledFocusRegion(*text)) {
                    action.focusTarget = FocusTarget::OledRegion;
                    action.focusX = region->x;
                    action.focusY = region->y;
                    action.focusWidth = region->width;
                    action.focusHeight = region->height;
                } else
                    issue(
                        *value, "focus.target: unknown focus target '" + *text + "'", sceneIndex,
                        actionIndex);
            }
        }
        if (const Node* value = node.find("label"))
            if (const auto parsed = scalar(*value, "focus.label", sceneIndex, actionIndex))
                action.text = *parsed;
        if (const Node* value = node.find("placement"))
            if (const auto parsed = scalar(*value, "focus.placement", sceneIndex, actionIndex)) {
                if (*parsed == "auto")
                    action.focusPlacement = FocusPlacement::Auto;
                else if (*parsed == "left")
                    action.focusPlacement = FocusPlacement::Left;
                else if (*parsed == "right")
                    action.focusPlacement = FocusPlacement::Right;
                else if (*parsed == "above")
                    action.focusPlacement = FocusPlacement::Above;
                else if (*parsed == "below")
                    action.focusPlacement = FocusPlacement::Below;
                else
                    issue(
                        *value, "focus.placement: expected auto/left/right/above/below", sceneIndex,
                        actionIndex);
            }
        const Node* focusDuration = node.find("duration_ms");
        if (focusDuration != nullptr) {
            if (const auto parsed = uintValue(
                    *focusDuration, "focus.duration_ms", 0U, 60000U, sceneIndex, actionIndex))
                action.durationMs = *parsed;
        } else
            action.durationMs = 2200U;
        if (action.focusTarget == FocusTarget::OledRegion && action.focusWidth == 0) {
            if (const Node* value = required(node, "x", "focus", sceneIndex, actionIndex))
                if (const auto parsed =
                        uintValue(*value, "focus.x", 0U, 127U, sceneIndex, actionIndex))
                    action.focusX = static_cast<int>(*parsed);
            if (const Node* value = required(node, "y", "focus", sceneIndex, actionIndex))
                if (const auto parsed =
                        uintValue(*value, "focus.y", 0U, 63U, sceneIndex, actionIndex))
                    action.focusY = static_cast<int>(*parsed);
            if (const Node* value = required(node, "width", "focus", sceneIndex, actionIndex))
                if (const auto parsed =
                        uintValue(*value, "focus.width", 1U, 128U, sceneIndex, actionIndex))
                    action.focusWidth = static_cast<int>(*parsed);
            if (const Node* value = required(node, "height", "focus", sceneIndex, actionIndex))
                if (const auto parsed =
                        uintValue(*value, "focus.height", 1U, 64U, sceneIndex, actionIndex))
                    action.focusHeight = static_cast<int>(*parsed);
        }
    } else if (name == "wait_until") {
        action.kind = StoryActionKind::WaitUntil;
        rejectUnknown(
            node, {"external_sync", "transport", "power", "timeout_ms"}, "wait_until", sceneIndex,
            actionIndex);
        const char* selected = nullptr;
        for (const char* key : {"external_sync", "transport", "power"})
            if (node.find(key) != nullptr)
                selected = selected == nullptr ? key : "";
        if (selected == nullptr || selected[0] == '\0')
            issue(node, "wait_until requires exactly one condition", sceneIndex, actionIndex);
        else {
            const Node* value = node.find(selected);
            action.waitCondition = std::string(selected) == "external_sync"
                ? WaitCondition::ExternalSync
                : std::string(selected) == "transport" ? WaitCondition::Transport
                                                       : WaitCondition::Power;
            if (value != nullptr)
                if (const auto parsed =
                        scalar(*value, "wait_until expected state", sceneIndex, actionIndex))
                    action.expected = *parsed;
        }
        action.timeoutMs = 3000U;
        if (const Node* value = node.find("timeout_ms"))
            if (const auto parsed = uintValue(
                    *value, "wait_until.timeout_ms", 1U, 3600000U, sceneIndex, actionIndex))
                action.timeoutMs = *parsed;
    } else if (name == "assert") {
        action.kind = StoryActionKind::Assert;
        rejectUnknown(
            node, {"clock_source", "transport", "power", "sync_cable", "rst_cable"}, "assert",
            sceneIndex, actionIndex);
        const char* selected = nullptr;
        for (const char* key : {"clock_source", "transport", "power", "sync_cable", "rst_cable"})
            if (node.find(key) != nullptr)
                selected = selected == nullptr ? key : "";
        if (selected == nullptr || selected[0] == '\0')
            issue(node, "assert requires exactly one assertion", sceneIndex, actionIndex);
        else {
            const std::string key(selected);
            action.assertion = key == "clock_source" ? AssertionKind::ClockSource
                : key == "transport"                 ? AssertionKind::Transport
                : key == "power"                     ? AssertionKind::Power
                : key == "sync_cable"                ? AssertionKind::SyncCable
                                                     : AssertionKind::ResetCable;
            const Node* value = node.find(selected);
            if (value != nullptr)
                if (const auto parsed =
                        scalar(*value, "assert expected state", sceneIndex, actionIndex))
                    action.expected = *parsed;
        }
    } else {
        issue(node, "unknown action type '" + name + "'", sceneIndex, actionIndex);
    }
    return action;
}

}  // namespace clockfw::sim::tutorial::detail
