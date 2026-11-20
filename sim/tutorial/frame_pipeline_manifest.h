/**
 * @file frame_pipeline_manifest.h
 * @brief JSON manifests written by the Storybook frame pipeline.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tutorial/frame_pipeline.h"
#include "tutorial/story_runner.h"
#include "tutorial/subtitle_writer.h"

namespace clockfw::sim::tutorial {

/** One narration cue of a Story run, in presentation time. */
struct NarrationCue final {
    std::string id;
    std::uint64_t startUs = 0ULL;
    std::uint64_t endUs = 0ULL;
};

/** @brief Pairs the narration begin/end events of a run trace; throws on unbalanced cues. */
std::vector<NarrationCue> narrationCuesFromTrace(const StoryRunResult& run);

/** @brief Serializes the narration cues of one Story as narration.json (schema 1). */
std::string narrationManifest(const Story& story, const std::vector<NarrationCue>& cues);

/** @brief Serializes the frame manifest.json (schema 1) of one frame-pipeline run. */
std::string makeManifest(
    const Story& story,
    const std::string& sourceRevision,
    const StoryRunResult& run,
    const std::vector<StoryFrameRecord>& frames,
    const std::vector<SubtitleCue>& subtitles,
    const std::vector<NarrationCue>& narration);

}  // namespace clockfw::sim::tutorial
