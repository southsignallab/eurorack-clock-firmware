/**
 * @file story_run_context.cpp
 * @brief Storybook run setup, scene sequencing and action execution.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "tutorial/story_run_context.h"

#include "tutorial/interaction_profile_loader.h"
#include "tutorial/story_narration_beat.h"
#include "tutorial/story_runner_support.h"
#include "tutorial/story_validator.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
namespace clockfw::sim::tutorial::detail {

RunContext::RunContext(
    StorySimulatorPort& port,
    const Story& story,
    const InteractionTiming& timing,
    StoryPresentationSink* presentationSink,
    StoryExecutionObserver* executionObserver)
    : port_(port), story_(story), timing_(timing), presentationSink_(presentationSink),
      executionObserver_(executionObserver) {}

StoryRunResult RunContext::execute() {
    if (!applySetup()) {
        finish();
        return std::move(result_);
    }
    for (std::size_t sceneIndex = 0U; sceneIndex < story_.scenes.size(); ++sceneIndex) {
        if (!executeScene(story_.scenes[sceneIndex], sceneIndex)) {
            break;
        }
    }
    finish();
    return std::move(result_);
}

bool RunContext::applySetup() {
    if (story_.setup.factoryReset) {
        const StoryPortResult reset = port_.resetPersistenceToFactory();
        if (!reset) {
            addIssue(
                std::nullopt, std::nullopt, 0U,
                std::string("setup factory_reset failed: ") + portErrorName(reset.error));
            return false;
        }
        emit(StoryTraceKind::Setup, std::nullopt, std::nullopt, "factory_reset", "applied");
    }
    if (story_.setup.powerOn.has_value()) {
        const StoryPortResult power = port_.setPower(*story_.setup.powerOn);
        if (!power) {
            addIssue(
                std::nullopt, std::nullopt, 0U,
                std::string("setup power failed: ") + portErrorName(power.error));
            return false;
        }
        emit(
            StoryTraceKind::Setup, std::nullopt, std::nullopt, "power",
            *story_.setup.powerOn ? "on" : "off");
    }
    return true;
}

bool RunContext::executeScene(const StoryScene& scene, const std::size_t sceneIndex) {
    currentScene_ = &scene;
    currentSceneIndex_ = sceneIndex;
    activeSubtitle_ = scene.kind == SceneKind::Tutorial ? scene.subtitle : std::string{};
    emit(
        StoryTraceKind::SceneBegin, sceneIndex, std::nullopt, traceSceneName(scene.kind),
        scene.title);
    if (scene.kind != SceneKind::Tutorial) {
        if (scene.narrationId)
            emit(StoryTraceKind::NarrationBegin, sceneIndex, std::nullopt, *scene.narrationId, {});
        advancePresentationMs(scene.durationMs);
        if (scene.narrationId)
            emit(StoryTraceKind::NarrationEnd, sceneIndex, std::nullopt, *scene.narrationId, {});
        emit(
            StoryTraceKind::SceneEnd, sceneIndex, std::nullopt, traceSceneName(scene.kind),
            scene.title);
        currentScene_ = nullptr;
        activeSubtitle_.clear();
        return true;
    }
    if (!scene.subtitle.empty()) {
        emit(StoryTraceKind::Subtitle, sceneIndex, std::nullopt, "subtitle", scene.subtitle);
    }
    for (std::size_t actionIndex = 0U; actionIndex < scene.actions.size(); ++actionIndex) {
        const StoryAction& action = scene.actions[actionIndex];
        emit(
            StoryTraceKind::ActionBegin, sceneIndex, actionIndex, traceActionName(action.kind), {});
        const bool beatBoundary =
            action.kind == StoryActionKind::BeatBegin || action.kind == StoryActionKind::BeatEnd;
        if (action.narrationId && !beatBoundary) {
            emit(StoryTraceKind::NarrationBegin, sceneIndex, actionIndex, *action.narrationId, {});
        }
        if (isPacedAction(action.kind)) {
            advanceActiveMs(timing_.beforeActionMs);
        }
        if (!executeAction(action, sceneIndex, actionIndex)) {
            return false;
        }
        if (action.narrationId && !beatBoundary) {
            emit(StoryTraceKind::NarrationEnd, sceneIndex, actionIndex, *action.narrationId, {});
        }
        emit(StoryTraceKind::ActionEnd, sceneIndex, actionIndex, traceActionName(action.kind), {});
    }
    emit(
        StoryTraceKind::SceneEnd, sceneIndex, std::nullopt, traceSceneName(scene.kind),
        scene.title);
    currentScene_ = nullptr;
    activeSubtitle_.clear();
    return true;
}

bool RunContext::executeAction(
    const StoryAction& action,
    const std::size_t sceneIndex,
    const std::size_t actionIndex) {
    switch (action.kind) {
        case StoryActionKind::BeatBegin:
            return beginBeat(action, sceneIndex, actionIndex);
        case StoryActionKind::BeatEnd:
            return endBeat(action, sceneIndex, actionIndex);
        case StoryActionKind::Encoder:
            for (std::uint16_t detent = 0U; detent < action.detents; ++detent) {
                if (!requirePort(
                        port_.rotateEncoderDetent(action.encoderDirection), action, sceneIndex,
                        actionIndex)) {
                    return false;
                }
                if (presentationSink_ != nullptr) {
                    presentationSink_->onEncoderDetent(presentationUs_, action.encoderDirection);
                }
                emit(
                    StoryTraceKind::EncoderDetent, sceneIndex, actionIndex, "encoder",
                    action.encoderDirection > 0 ? "clockwise" : "counter_clockwise");
                advanceActiveMs(
                    action.encoderFast ? timing_.encoderFastDetentMs : timing_.encoderDetentMs);
            }
            advanceActiveMs(timing_.afterValueChangeMs);
            return true;
        case StoryActionKind::EncoderPush:
            return executeButton(
                ModuleControl::EncoderPush, action.holdMs.value_or(timing_.encoderPushDownMs),
                timing_.encoderPushReleasePauseMs + timing_.afterNavigationMs, action, sceneIndex,
                actionIndex);
        case StoryActionKind::Button:
            if (action.buttonState.has_value()) {
                if (!requirePort(
                        port_.setModuleControl(action.moduleControl, *action.buttonState), action,
                        sceneIndex, actionIndex)) {
                    return false;
                }
                if (presentationSink_ != nullptr) {
                    presentationSink_->onControlState(
                        presentationUs_, action.moduleControl, *action.buttonState);
                }
                emit(
                    StoryTraceKind::ControlState, sceneIndex, actionIndex,
                    std::string(moduleControlName(action.moduleControl)),
                    *action.buttonState ? "down" : "up");
                advanceActiveMs(
                    *action.buttonState ? timing_.buttonDownMs : timing_.buttonReleasePauseMs);
                return true;
            }
            return executeButton(
                action.moduleControl, timing_.buttonDownMs, timing_.buttonReleasePauseMs, action,
                sceneIndex, actionIndex);
        case StoryActionKind::Power: {
            if (!requirePort(port_.setPower(action.state), action, sceneIndex, actionIndex))
                return false;
            if (presentationSink_ != nullptr) {
                presentationSink_->onPowerState(presentationUs_, action.state);
            }
            emit(
                StoryTraceKind::PowerState, sceneIndex, actionIndex, "power",
                action.state ? "on" : "off");
            advanceActiveMs(timing_.afterMajorScreenChangeMs);
            return true;
        }
        case StoryActionKind::SyncCable:
        case StoryActionKind::ResetCable: {
            const PatchAction patch = action.kind == StoryActionKind::SyncCable
                ? PatchAction::SyncCable
                : PatchAction::ResetCable;
            if (!requirePort(
                    port_.setPatchConnected(patch, action.state), action, sceneIndex, actionIndex))
                return false;
            if (presentationSink_ != nullptr) {
                presentationSink_->onPatchMotion(
                    presentationUs_, patch, action.state,
                    static_cast<std::uint64_t>(timing_.patchActionMs) * kUsPerMs);
            }
            emit(
                StoryTraceKind::PatchState, sceneIndex, actionIndex,
                action.kind == StoryActionKind::SyncCable ? "sync_cable" : "rst_cable",
                action.state ? "connected" : "disconnected");
            advanceActiveMs(timing_.patchActionMs);
            if (presentationSink_ != nullptr) {
                presentationSink_->onPatchSettled(presentationUs_, patch, action.state);
            }
            return true;
        }
        case StoryActionKind::SyncGenerator:
        case StoryActionKind::ResetGenerator: {
            const ExternalStimulus stimulus = action.kind == StoryActionKind::SyncGenerator
                ? ExternalStimulus::SyncGenerator
                : ExternalStimulus::ResetGenerator;
            if (!requirePort(
                    port_.setGeneratorRunning(stimulus, action.state), action, sceneIndex,
                    actionIndex))
                return false;
            emit(
                StoryTraceKind::GeneratorState, sceneIndex, actionIndex,
                action.kind == StoryActionKind::SyncGenerator ? "sync_generator" : "rst_generator",
                action.state ? "run" : "hold");
            advanceActiveMs(timing_.afterValueChangeMs);
            return true;
        }
        case StoryActionKind::SyncSource: {
            const SyncInputTelemetry current = port_.syncTelemetry();
            const std::uint32_t bpmMilli = action.bpm ? (*action.bpm * 1000U) : current.bpmMilli;
            const std::uint8_t ppqn = action.ppqn.value_or(current.ppqn);
            const SignalWaveform waveform =
                action.waveform ? toSimulatorWaveform(*action.waveform) : current.waveform;
            if (!requirePort(
                    port_.configureSyncSource(bpmMilli, ppqn, waveform), action, sceneIndex,
                    actionIndex))
                return false;
            emit(
                StoryTraceKind::SourceConfigured, sceneIndex, actionIndex, "sync_source",
                std::to_string(bpmMilli) + "mBPM/" + std::to_string(ppqn) + "PPQN");
            advanceActiveMs(timing_.afterValueChangeMs);
            return true;
        }
        case StoryActionKind::ResetPulse:
            if (!requirePort(port_.triggerResetPulse(), action, sceneIndex, actionIndex))
                return false;
            emit(StoryTraceKind::ResetPulse, sceneIndex, actionIndex, "rst_pulse", "triggered");
            advanceActiveMs(timing_.afterValueChangeMs);
            return true;
        case StoryActionKind::Subtitle:
            activeSubtitle_ = action.text;
            emit(StoryTraceKind::Subtitle, sceneIndex, actionIndex, "subtitle", action.text);
            return true;
        case StoryActionKind::Scope:
            if (presentationSink_ != nullptr) {
                presentationSink_->onScopeState(presentationUs_, action.scopeMode);
            }
            emit(
                StoryTraceKind::Scope, sceneIndex, actionIndex, "scope",
                action.scopeMode == ScopeMode::VisibleChannel ? "visible_channel" : "hidden");
            return true;
        case StoryActionKind::Focus:
            if (presentationSink_ != nullptr)
                presentationSink_->onFocusState(
                    presentationUs_, action.focusTarget, true, action.focusPlacement, action.focusX,
                    action.focusY, action.focusWidth, action.focusHeight, action.text);
            emit(StoryTraceKind::Focus, sceneIndex, actionIndex, "focus", action.text);
            if (action.durationMs == 0U && beat_.active()) {
                beat_.retainFocus(action.focusTarget);
                return true;
            }
            advancePresentationMs(action.durationMs);
            if (presentationSink_ != nullptr)
                presentationSink_->onFocusState(
                    presentationUs_, action.focusTarget, false, FocusPlacement::Auto, 0, 0, 0, 0,
                    {});
            return true;
        case StoryActionKind::Wait:
            advanceActiveMs(action.durationMs);
            emit(
                StoryTraceKind::WaitSatisfied, sceneIndex, actionIndex, "wait_ms",
                std::to_string(action.durationMs));
            return true;
        case StoryActionKind::WaitUntil:
            return executeWaitUntil(action, sceneIndex, actionIndex);
        case StoryActionKind::Assert:
            if (!assertionMatches(action)) {
                addIssue(
                    sceneIndex, actionIndex, action.sourceLine,
                    "assertion failed: expected " + action.expected);
                return false;
            }
            emit(
                StoryTraceKind::AssertionPassed, sceneIndex, actionIndex, "assert",
                action.expected);
            return true;
    }
    return false;
}

}  // namespace clockfw::sim::tutorial::detail
