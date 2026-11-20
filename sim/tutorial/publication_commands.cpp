/**
 * @file publication_commands.cpp
 * @brief ffmpeg command lines of the Storybook publication stage.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "tutorial/publication_commands.h"

#include <iomanip>
#include <sstream>

namespace clockfw::sim::tutorial {
namespace {

std::string videoFilter(const Story& story) {
    return "scale=" + std::to_string(story.output.width) + ":" +
        std::to_string(story.output.height) +
        ":force_original_aspect_ratio=decrease,pad=" + std::to_string(story.output.width) + ":" +
        std::to_string(story.output.height) +
        ":(ow-iw)/2:(oh-ih)/2,fps=" + std::to_string(story.output.framesPerSecond) +
        ",format=yuv420p";
}

std::string durationSeconds(const std::uint64_t durationUs) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6) << (static_cast<double>(durationUs) / 1000000.0);
    return out.str();
}

}  // namespace

std::vector<std::string> normalizedClipCommand(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& input,
    const std::filesystem::path& output,
    const Story& story,
    const std::uint64_t durationUs,
    const bool inputHasAudio,
    const bool requireAudio) {
    std::vector<std::string> args{ffmpeg.string(), "-y", "-v", "error", "-i", input.string()};
    if (requireAudio && !inputHasAudio) {
        args.insert(
            args.end(), {"-f", "lavfi", "-i", "anullsrc=channel_layout=stereo:sample_rate=48000"});
    }
    args.insert(
        args.end(),
        {"-map", "0:v:0", "-vf", videoFilter(story), "-c:v", "libx264", "-preset", "medium", "-crf",
         "18", "-pix_fmt", "yuv420p"});
    if (requireAudio) {
        if (inputHasAudio)
            args.insert(args.end(), {"-map", "0:a:0", "-af", "apad"});
        else
            args.insert(args.end(), {"-map", "1:a:0"});
        args.insert(args.end(), {"-c:a", "aac", "-ar", "48000", "-ac", "2", "-shortest"});
    } else {
        args.push_back("-an");
    }
    args.insert(args.end(), {"-t", durationSeconds(durationUs)});
    args.push_back(output.string());
    return args;
}

std::vector<std::string> tutorialClipCommand(
    const std::filesystem::path& ffmpeg,
    const Story& story,
    const StoryFramePipelineResult& generated,
    const std::filesystem::path& concatInput,
    const std::filesystem::path& output,
    const bool requireAudio) {
    std::vector<std::string> args{ffmpeg.string(), "-y",    "-v", "error", "-f",
                                  "concat",        "-safe", "0",  "-i",    concatInput.string()};
    if (requireAudio) {
        args.insert(
            args.end(), {"-f", "lavfi", "-i", "anullsrc=channel_layout=stereo:sample_rate=48000"});
    }
    args.insert(
        args.end(),
        {"-map", "0:v:0", "-vf",
         "fps=" + std::to_string(story.output.framesPerSecond) + ",format=yuv420p", "-c:v",
         "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p"});
    if (requireAudio) {
        args.insert(
            args.end(), {"-map", "1:a:0", "-c:a", "aac", "-ar", "48000", "-ac", "2", "-shortest"});
    } else {
        args.push_back("-an");
    }
    const std::uint64_t logicalDurationUs =
        (static_cast<std::uint64_t>(generated.frames.size()) * 1000000ULL) /
        static_cast<std::uint64_t>(story.output.framesPerSecond);
    args.insert(args.end(), {"-t", durationSeconds(logicalDurationUs), output.string()});
    return args;
}

std::vector<std::string> concatCommand(
    const std::filesystem::path& ffmpeg,
    const std::vector<std::filesystem::path>& inputs,
    const std::filesystem::path& output,
    const bool withAudio) {
    std::vector<std::string> args{ffmpeg.string(), "-y", "-v", "error"};
    for (const auto& input : inputs)
        args.insert(args.end(), {"-i", input.string()});
    std::ostringstream filter;
    for (std::size_t index = 0U; index < inputs.size(); ++index) {
        filter << '[' << index << ":v:0]";
        if (withAudio)
            filter << '[' << index << ":a:0]";
    }
    filter << "concat=n=" << inputs.size() << ":v=1:a=" << (withAudio ? 1 : 0) << "[v]";
    if (withAudio)
        filter << "[a]";
    args.insert(args.end(), {"-filter_complex", filter.str(), "-map", "[v]"});
    if (withAudio)
        args.insert(args.end(), {"-map", "[a]"});
    args.insert(
        args.end(), {"-c:v", "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p"});
    if (withAudio)
        args.insert(args.end(), {"-c:a", "aac", "-ar", "48000", "-ac", "2"});
    else
        args.push_back("-an");
    args.insert(args.end(), {"-movflags", "+faststart", output.string()});
    return args;
}

}  // namespace clockfw::sim::tutorial
