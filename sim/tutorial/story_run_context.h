/**
 * @file story_run_context.h
 * @brief Execution state of one Storybook run: setup, scenes, actions, waits and the trace.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "tutorial/interaction_profile_loader.h"
#include "tutorial/story_narration_beat.h"
#include "tutorial/story_runner.h"

namespace clockfw::sim::tutorial::detail {

/** Microseconds per millisecond. */
inline constexpr std::uint64_t kUsPerMs = 1000ULL;
/** Poll interval of wait_until conditions, in microseconds. */
inline constexpr std::uint64_t kWaitPollUs = 1000ULL;

/** @brief Executes one validated Story against the simulator port and records its trace. */
class RunContext final {
  public:
    RunContext(
        StorySimulatorPort& port,
        const Story& story,
        const InteractionTiming& timing,
        StoryPresentationSink* presentationSink,
        StoryExecutionObserver* executionObserver);
    StoryRunResult execute();

  private:
    bool applySetup();
    bool executeScene(const StoryScene& scene, const std::size_t sceneIndex);
    bool executeAction(
        const StoryAction& action,
        const std::size_t sceneIndex,
        const std::size_t actionIndex);
    bool beginBeat(
        const StoryAction& action,
        const std::size_t sceneIndex,
        const std::size_t actionIndex);
    bool endBeat(
        const StoryAction& action,
        const std::size_t sceneIndex,
        const std::size_t actionIndex);
    bool executeButton(
        const ModuleControl control,
        const std::uint32_t downMs,
        const std::uint32_t releaseAndPauseMs,
        const StoryAction& action,
        const std::size_t sceneIndex,
        const std::size_t actionIndex);
    bool executeWaitUntil(
        const StoryAction& action,
        const std::size_t sceneIndex,
        const std::size_t actionIndex);
    bool waitConditionMatches(const StoryAction& action) const;
    bool assertionMatches(const StoryAction& action) const;
    bool transportMatches(const std::string& expected) const;
    bool requirePort(
        const StoryPortResult result,
        const StoryAction& action,
        const std::size_t sceneIndex,
        const std::size_t actionIndex);
    void addIssue(
        const std::optional<std::size_t> sceneIndex,
        const std::optional<std::size_t> actionIndex,
        const std::size_t sourceLine,
        std::string reason);
    void emit(
        const StoryTraceKind kind,
        const std::optional<std::size_t> sceneIndex,
        const std::optional<std::size_t> actionIndex,
        std::string name,
        std::string value);
    void sampleUntil(const std::uint64_t targetPresentationUs, const bool firmwareAdvances);
    void advancePresentationMs(const std::uint32_t durationMs);
    void advanceActiveMs(const std::uint32_t durationMs);
    void advanceActiveUs(const std::uint64_t durationUs);
    void finish();
    StorySimulatorPort& port_;
    const Story& story_;
    const InteractionTiming& timing_;
    StoryRunResult result_;
    std::uint64_t presentationUs_ = 0ULL;
    StoryPresentationSink* presentationSink_ = nullptr;
    StoryExecutionObserver* executionObserver_ = nullptr;
    const StoryScene* currentScene_ = nullptr;
    std::optional<std::size_t> currentSceneIndex_;
    std::string activeSubtitle_;
    StoryNarrationBeat beat_;
};

}  // namespace clockfw::sim::tutorial::detail
