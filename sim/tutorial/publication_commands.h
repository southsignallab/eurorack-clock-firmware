/**
 * @file publication_commands.h
 * @brief ffmpeg command lines of the Storybook publication stage.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "tutorial/frame_pipeline.h"
#include "tutorial/story_model.h"

namespace clockfw::sim::tutorial {

/** @brief Normalizes an intro/outro clip to the Story geometry and frame rate (H.264, optional AAC). */
std::vector<std::string> normalizedClipCommand(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& input,
    const std::filesystem::path& output,
    const Story& story,
    std::uint64_t durationUs,
    bool inputHasAudio,
    bool requireAudio);

/** @brief Encodes the sparse tutorial frame concat list into one clip of the logical Story duration. */
std::vector<std::string> tutorialClipCommand(
    const std::filesystem::path& ffmpeg,
    const Story& story,
    const StoryFramePipelineResult& generated,
    const std::filesystem::path& concatInput,
    const std::filesystem::path& output,
    bool requireAudio);

/** @brief Concatenates normalized segments into the H.264 publication video. */
std::vector<std::string> concatCommand(
    const std::filesystem::path& ffmpeg,
    const std::vector<std::filesystem::path>& inputs,
    const std::filesystem::path& output,
    bool withAudio);

}  // namespace clockfw::sim::tutorial
