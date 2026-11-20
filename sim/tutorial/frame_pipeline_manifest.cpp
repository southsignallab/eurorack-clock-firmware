/**
 * @file frame_pipeline_manifest.cpp
 * @brief JSON manifests written by the Storybook frame pipeline.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "tutorial/frame_pipeline_manifest.h"

#include <filesystem>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "version.h"

namespace clockfw::sim::tutorial {
namespace {

std::string jsonEscape(const std::string& text) {
    std::ostringstream out;
    for (const char raw : text) {
        const auto value = static_cast<unsigned char>(raw);
        switch (value) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (value < 0x20U) {
                    out << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                        << static_cast<unsigned>(value) << std::dec;
                } else {
                    out << static_cast<char>(value);
                }
                break;
        }
    }
    return out.str();
}

}  // namespace

std::vector<NarrationCue> narrationCuesFromTrace(const StoryRunResult& run) {
    std::vector<NarrationCue> cues;
    std::unordered_map<std::string, std::uint64_t> open;
    for (const auto& event : run.trace) {
        if (event.kind == StoryTraceKind::NarrationBegin) {
            if (!open.emplace(event.name, event.presentationUs).second) {
                throw std::runtime_error("duplicate open narration cue: " + event.name);
            }
        } else if (event.kind == StoryTraceKind::NarrationEnd) {
            const auto found = open.find(event.name);
            if (found == open.end())
                throw std::runtime_error("narration cue ended without begin: " + event.name);
            cues.push_back({event.name, found->second, event.presentationUs});
            open.erase(found);
        }
    }
    if (!open.empty())
        throw std::runtime_error("narration cue did not close: " + open.begin()->first);
    return cues;
}

std::string narrationManifest(const Story& story, const std::vector<NarrationCue>& cues) {
    std::ostringstream out;
    out << "{\n  \"schema\": 1,\n  \"story_id\": \"" << jsonEscape(story.id)
        << "\",\n  \"cues\": [\n";
    for (std::size_t index = 0U; index < cues.size(); ++index) {
        const auto& cue = cues[index];
        out << "    {\"id\": \"" << jsonEscape(cue.id) << "\", \"start_us\": " << cue.startUs
            << ", \"end_us\": " << cue.endUs << "}";
        if (index + 1U < cues.size())
            out << ',';
        out << '\n';
    }
    out << "  ]\n}\n";
    return out.str();
}

std::string makeManifest(
    const Story& story,
    const std::string& sourceRevision,
    const StoryRunResult& run,
    const std::vector<StoryFrameRecord>& frames,
    const std::vector<SubtitleCue>& subtitles,
    const std::vector<NarrationCue>& narration) {
    std::ostringstream out;
    out << "{\n"
        << "  \"manifest_schema\": 1,\n"
        << "  \"story_schema\": " << story.schema << ",\n"
        << "  \"story_id\": \"" << jsonEscape(story.id) << "\",\n"
        << "  \"story_title\": \"" << jsonEscape(story.title) << "\",\n"
        << "  \"firmware_version\": \"" << CLOCK_FIRMWARE_VERSION << "\",\n"
        << "  \"simulator_source_revision\": \"" << jsonEscape(sourceRevision) << "\",\n"
        << "  \"theme\": \"" << jsonEscape(story.theme) << "\",\n"
        << "  \"interaction_profile\": \"" << interactionProfileName(story.interactionProfile)
        << "\",\n"
        << "  \"output\": {\"width\": " << story.output.width
        << ", \"height\": " << story.output.height << ", \"fps\": " << story.output.framesPerSecond
        << "},\n"
        << "  \"presentation_duration_us\": " << run.presentationDurationUs << ",\n"
        << "  \"frame_count\": " << frames.size() << ",\n"
        << "  \"unique_frame_file_count\": " <<
        [&]() {
            std::set<std::filesystem::path> paths;
            for (const auto& frame : frames)
                paths.insert(frame.relativePath);
            return paths.size();
        }()
        << ",\n"
        << "  \"subtitle_count\": " << subtitles.size() << ",\n"
        << "  \"narration_cue_count\": " << narration.size() << ",\n"
        << "  \"frame_hash_algorithm\": \"fnv1a64-rgba8\",\n"
        << "  \"frames\": [\n";
    for (std::size_t index = 0U; index < frames.size(); ++index) {
        const StoryFrameRecord& frame = frames[index];
        out << "    {\"index\": " << frame.index
            << ", \"presentation_us\": " << frame.presentationUs << ", \"path\": \""
            << jsonEscape(frame.relativePath.generic_string()) << "\", \"rgba_fnv1a64\": \""
            << frame.rgbaFnv1a64 << "\"}";
        if (index + 1U < frames.size())
            out << ',';
        out << '\n';
    }
    out << "  ]\n}\n";
    return out.str();
}

}  // namespace clockfw::sim::tutorial
