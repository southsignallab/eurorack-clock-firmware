/**
 * @file story_runner.cpp
 * @brief Implements Storybook narration beats, waits, assertions and presentation sampling, and StoryRunner.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "tutorial/story_runner.h"
#include "tutorial/interaction_profile_loader.h"
#include "tutorial/story_narration_beat.h"
#include "tutorial/story_run_context.h"
#include "tutorial/story_runner_support.h"
#include "tutorial/story_validator.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
namespace clockfw::sim::tutorial {
namespace detail {

bool RunContext::beginBeat(
    const StoryAction& action,
    const std::size_t sceneIndex,
    const std::size_t actionIndex) {
    std::string error;
    if (!beat_.begin(action, presentationUs_, error)) {
        addIssue(sceneIndex, actionIndex, action.sourceLine, std::move(error));
        return false;
    }
    emit(StoryTraceKind::NarrationBegin, sceneIndex, actionIndex, *action.narrationId, {});
    return true;
}

bool RunContext::endBeat(
    const StoryAction& action,
    const std::size_t sceneIndex,
    const std::size_t actionIndex) {
    StoryNarrationBeatFinish finish;
    std::string error;
    if (!beat_.finish(action, presentationUs_, finish, error)) {
        addIssue(sceneIndex, actionIndex, action.sourceLine, std::move(error));
        return false;
    }
    if (finish.remainingUs > 0ULL)
        sampleUntil(presentationUs_ + finish.remainingUs, false);
    if (finish.focusToClear && presentationSink_ != nullptr)
        presentationSink_->onFocusState(
            presentationUs_, *finish.focusToClear, false, FocusPlacement::Auto, 0, 0, 0, 0, {});
    emit(StoryTraceKind::NarrationEnd, sceneIndex, actionIndex, finish.id, {});
    return true;
}

bool RunContext::executeButton(
    const ModuleControl control,
    const std::uint32_t downMs,
    const std::uint32_t releaseAndPauseMs,
    const StoryAction& action,
    const std::size_t sceneIndex,
    const std::size_t actionIndex) {
    if (!requirePort(port_.setModuleControl(control, true), action, sceneIndex, actionIndex))
        return false;
    if (presentationSink_ != nullptr) {
        presentationSink_->onControlState(presentationUs_, control, true);
    }
    emit(
        StoryTraceKind::ControlState, sceneIndex, actionIndex,
        std::string(moduleControlName(control)), "down");
    advanceActiveMs(downMs);
    if (!requirePort(port_.setModuleControl(control, false), action, sceneIndex, actionIndex))
        return false;
    if (presentationSink_ != nullptr) {
        presentationSink_->onControlState(presentationUs_, control, false);
    }
    emit(
        StoryTraceKind::ControlState, sceneIndex, actionIndex,
        std::string(moduleControlName(control)), "up");
    advanceActiveMs(releaseAndPauseMs);
    return true;
}

bool RunContext::executeWaitUntil(
    const StoryAction& action,
    const std::size_t sceneIndex,
    const std::size_t actionIndex) {
    if (waitConditionMatches(action)) {
        emit(StoryTraceKind::WaitSatisfied, sceneIndex, actionIndex, "wait_until", action.expected);
        return true;
    }
    std::uint64_t remainingUs = static_cast<std::uint64_t>(action.timeoutMs) * kUsPerMs;
    while (remainingUs > 0ULL) {
        const std::uint64_t stepUs = std::min(kWaitPollUs, remainingUs);
        advanceActiveUs(stepUs);
        remainingUs -= stepUs;
        if (waitConditionMatches(action)) {
            emit(
                StoryTraceKind::WaitSatisfied, sceneIndex, actionIndex, "wait_until",
                action.expected);
            return true;
        }
    }
    addIssue(
        sceneIndex, actionIndex, action.sourceLine,
        "wait_until timeout: expected " + action.expected);
    return false;
}

bool RunContext::waitConditionMatches(const StoryAction& action) const {
    if (!action.waitCondition)
        return false;
    switch (*action.waitCondition) {
        case WaitCondition::ExternalSync:
            return port_.syncTelemetry().locked == (action.expected == "locked");
        case WaitCondition::Transport:
            return transportMatches(action.expected);
        case WaitCondition::Power:
            return port_.poweredOn() == (action.expected == "on");
    }
    return false;
}

bool RunContext::assertionMatches(const StoryAction& action) const {
    if (!action.assertion)
        return false;
    switch (*action.assertion) {
        case AssertionKind::ClockSource:
            if (action.expected == "auto")
                return port_.clockSource() == ClockSource::Auto;
            if (action.expected == "internal")
                return port_.clockSource() == ClockSource::Internal;
            if (action.expected == "external")
                return port_.clockSource() == ClockSource::External;
            return false;
        case AssertionKind::Transport:
            return transportMatches(action.expected);
        case AssertionKind::Power:
            return port_.poweredOn() == (action.expected == "on");
        case AssertionKind::SyncCable:
            return port_.syncTelemetry().cableConnected == (action.expected == "connected");
        case AssertionKind::ResetCable:
            return port_.resetTelemetry().cableConnected == (action.expected == "connected");
    }
    return false;
}

bool RunContext::transportMatches(const std::string& expected) const {
    if (expected == "playing")
        return port_.transportState() == TransportState::Playing;
    if (expected == "paused")
        return port_.transportState() == TransportState::Paused;
    if (expected == "stopped")
        return port_.transportState() == TransportState::Stopped;
    return false;
}

bool RunContext::requirePort(
    const StoryPortResult result,
    const StoryAction& action,
    const std::size_t sceneIndex,
    const std::size_t actionIndex) {
    if (result)
        return true;
    addIssue(
        sceneIndex, actionIndex, action.sourceLine,
        std::string(traceActionName(action.kind)) + " failed: " + portErrorName(result.error));
    return false;
}

void RunContext::addIssue(
    const std::optional<std::size_t> sceneIndex,
    const std::optional<std::size_t> actionIndex,
    const std::size_t sourceLine,
    std::string reason) {
    result_.issues.push_back({story_.id, sceneIndex, actionIndex, sourceLine, std::move(reason)});
}

void RunContext::emit(
    const StoryTraceKind kind,
    const std::optional<std::size_t> sceneIndex,
    const std::optional<std::size_t> actionIndex,
    std::string name,
    std::string value) {
    result_.trace.push_back(
        {kind, presentationUs_, port_.runtime().nowMicroseconds(), sceneIndex, actionIndex,
         std::move(name), std::move(value)});
}

void RunContext::sampleUntil(
    const std::uint64_t targetPresentationUs,
    const bool firmwareAdvances) {
    while (executionObserver_ != nullptr) {
        const std::optional<std::uint64_t> next = executionObserver_->nextPresentationSampleUs();
        if (!next.has_value() || *next >= targetPresentationUs)
            break;
        if (*next < presentationUs_) {
            throw std::runtime_error(
                "Story execution observer requested a past presentation timestamp");
        }
        const std::uint64_t deltaUs = *next - presentationUs_;
        if (firmwareAdvances && deltaUs > 0ULL) {
            port_.advanceFirmwareMicroseconds(deltaUs);
        }
        presentationUs_ = *next;
        if (currentScene_ == nullptr || !currentSceneIndex_.has_value()) {
            throw std::runtime_error("Story execution observer sampled outside a scene");
        }
        executionObserver_->onPresentationSample(
            {presentationUs_, port_.runtime().nowMicroseconds(), *currentSceneIndex_, currentScene_,
             activeSubtitle_, &port_.presentationRuntime()});
    }
    const std::uint64_t remainingUs = targetPresentationUs - presentationUs_;
    if (firmwareAdvances && remainingUs > 0ULL) {
        port_.advanceFirmwareMicroseconds(remainingUs);
    }
    presentationUs_ = targetPresentationUs;
}

void RunContext::advancePresentationMs(const std::uint32_t durationMs) {
    const std::uint64_t target =
        presentationUs_ + static_cast<std::uint64_t>(durationMs) * kUsPerMs;
    sampleUntil(target, false);
}

void RunContext::advanceActiveMs(const std::uint32_t durationMs) {
    advanceActiveUs(static_cast<std::uint64_t>(durationMs) * kUsPerMs);
}

void RunContext::advanceActiveUs(const std::uint64_t durationUs) {
    if (durationUs == 0ULL)
        return;
    sampleUntil(presentationUs_ + durationUs, true);
}

void RunContext::finish() {
    result_.presentationDurationUs = presentationUs_;
}

}  // namespace detail
StoryRunResult::operator bool() const {
    return issues.empty();
}
StoryRunner::StoryRunner(
    StorySimulatorPort& port,
    std::filesystem::path tutorialRoot,
    StoryPresentationSink* presentationSink,
    StoryExecutionObserver* executionObserver)
    : port_(port), tutorialRoot_(std::move(tutorialRoot)), presentationSink_(presentationSink),
      executionObserver_(executionObserver) {}
StoryRunResult StoryRunner::run(const Story& story) {
    StoryRunResult rejected;
    rejected.issues = validateStory(story, tutorialRoot_);
    if (!rejected.issues.empty()) {
        return rejected;
    }
    const InteractionTimingResult timing =
        loadInteractionTiming(tutorialRoot_, story.interactionProfile);
    if (!timing) {
        rejected.issues = timing.issues;
        for (StoryIssue& issue : rejected.issues) {
            issue.storyId = story.id;
        }
        return rejected;
    }
    detail::RunContext context(port_, story, *timing.timing, presentationSink_, executionObserver_);
    return context.execute();
}
}  // namespace clockfw::sim::tutorial
