/**
 * @file story_decoder.cpp
 * @brief Storybook field, setup, scene and story-root decoding.
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

void StoryDecoder::issue(
    const Node& node,
    const std::string& reason,
    std::optional<std::size_t> scene,
    std::optional<std::size_t> action) {
    issues.push_back({story.id, scene, action, node.line, reason});
}

bool StoryDecoder::mapping(
    const Node& node,
    const char* context,
    std::optional<std::size_t> scene,
    std::optional<std::size_t> action) {
    if (node.type == Node::Type::Mapping)
        return true;
    issue(node, std::string(context) + " must be a mapping", scene, action);
    return false;
}

void StoryDecoder::rejectUnknown(
    const Node& node,
    std::initializer_list<const char*> allowed,
    const char* context,
    std::optional<std::size_t> scene,
    std::optional<std::size_t> action) {
    std::unordered_set<std::string> keys;
    for (const char* key : allowed) {
        keys.emplace(key);
    }
    for (const auto& entry : node.mapping) {
        if (keys.count(entry.first) == 0U) {
            issue(
                entry.second, std::string(context) + ": unknown field '" + entry.first + "'", scene,
                action);
        }
    }
}

const Node* StoryDecoder::required(
    const Node& node,
    const char* key,
    const char* context,
    std::optional<std::size_t> scene,
    std::optional<std::size_t> action) {
    const Node* value = node.find(key);
    if (value == nullptr)
        issue(node, std::string(context) + ": missing required field '" + key + "'", scene, action);
    return value;
}

std::optional<std::string> StoryDecoder::scalar(
    const Node& node,
    const char* context,
    std::optional<std::size_t> scene,
    std::optional<std::size_t> action) {
    if (node.type != Node::Type::Scalar || node.scalar.empty()) {
        issue(node, std::string(context) + " must be a non-empty scalar", scene, action);
        return std::nullopt;
    }
    return node.scalar;
}

std::optional<std::uint32_t> StoryDecoder::uintValue(
    const Node& node,
    const char* context,
    const std::uint32_t min,
    const std::uint32_t max,
    std::optional<std::size_t> scene,
    std::optional<std::size_t> action) {
    const auto text = scalar(node, context, scene, action);
    if (!text)
        return std::nullopt;
    std::uint32_t value = 0U;
    const auto* begin = text->data();
    const auto* end = begin + text->size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end || value < min || value > max) {
        issue(
            node,
            std::string(context) + " must be an integer in range " + std::to_string(min) + ".." +
                std::to_string(max),
            scene, action);
        return std::nullopt;
    }
    return value;
}

std::optional<bool> StoryDecoder::boolValue(
    const Node& node,
    const char* context,
    std::optional<std::size_t> scene,
    std::optional<std::size_t> action) {
    const auto text = scalar(node, context, scene, action);
    if (!text) {
        return std::nullopt;
    }
    if (*text == "true")
        return true;
    if (*text == "false")
        return false;
    issue(node, std::string(context) + " must be true or false", scene, action);
    return std::nullopt;
}

std::optional<TransitionKind> StoryDecoder::transition(
    const Node& node,
    std::optional<std::size_t> scene) {
    if (!mapping(node, "transition", scene))
        return std::nullopt;
    rejectUnknown(node, {"type", "duration_ms"}, "transition", scene);
    const Node* type = required(node, "type", "transition", scene);
    if (type == nullptr)
        return std::nullopt;
    const auto text = scalar(*type, "transition.type", scene);
    if (!text)
        return std::nullopt;
    if (*text == "cut")
        return TransitionKind::Cut;
    if (*text == "fade")
        return TransitionKind::Fade;
    issue(*type, "transition.type: unsupported transition '" + *text + "'", scene);
    return std::nullopt;
}

std::optional<InteractionProfile> StoryDecoder::profile(const Node& node) {
    const auto text = scalar(node, "interaction_profile");
    if (!text)
        return std::nullopt;
    if (*text == "HUMAN_SLOW")
        return InteractionProfile::HumanSlow;
    if (*text == "HUMAN_NORMAL")
        return InteractionProfile::HumanNormal;
    if (*text == "HUMAN_FAST")
        return InteractionProfile::HumanFast;
    issue(node, "interaction_profile: unknown profile '" + *text + "'");
    return std::nullopt;
}

std::optional<StoryWaveform> StoryDecoder::waveform(
    const Node& node,
    std::size_t scene,
    std::size_t action) {
    const auto text = scalar(node, "waveform", scene, action);
    if (!text)
        return std::nullopt;
    if (*text == "square")
        return StoryWaveform::Square;
    if (*text == "sine")
        return StoryWaveform::Sine;
    if (*text == "triangle")
        return StoryWaveform::Triangle;
    issue(node, "waveform: expected square, sine or triangle", scene, action);
    return std::nullopt;
}

void StoryDecoder::decodeOutput(const Node& node) {
    if (!mapping(node, "output"))
        return;
    rejectUnknown(node, {"width", "height", "fps"}, "output");
    if (const Node* value = node.find("width")) {
        if (const auto parsed = uintValue(*value, "output.width", 16U, 16384U))
            story.output.width = static_cast<std::uint16_t>(*parsed);
    }
    if (const Node* value = node.find("height")) {
        if (const auto parsed = uintValue(*value, "output.height", 16U, 16384U))
            story.output.height = static_cast<std::uint16_t>(*parsed);
    }
    if (const Node* value = node.find("fps")) {
        if (const auto parsed = uintValue(*value, "output.fps", 1U, 240U))
            story.output.framesPerSecond = static_cast<std::uint16_t>(*parsed);
    }
}

void StoryDecoder::decodePublication(const Node& node) {
    if (!mapping(node, "publication"))
        return;
    rejectUnknown(node, {"intro_video", "outro_video"}, "publication");
    if (const Node* value = node.find("intro_video"))
        story.publication.introVideo = scalar(*value, "publication.intro_video");
    if (const Node* value = node.find("outro_video"))
        story.publication.outroVideo = scalar(*value, "publication.outro_video");
}

void StoryDecoder::decodeSetup(const Node& node) {
    if (!mapping(node, "setup"))
        return;
    rejectUnknown(node, {"factory_reset", "power"}, "setup");
    if (const Node* value = node.find("factory_reset")) {
        if (const auto parsed = boolValue(*value, "setup.factory_reset"))
            story.setup.factoryReset = *parsed;
    }
    if (const Node* value = node.find("power")) {
        const auto text = scalar(*value, "setup.power");
        if (text && (*text == "on" || *text == "off"))
            story.setup.powerOn = (*text == "on");
        else if (text)
            issue(*value, "setup.power must be 'on' or 'off'");
    }
}

StoryScene StoryDecoder::decodeScene(
    const std::string& name,
    const Node& node,
    const std::size_t sceneIndex) {
    StoryScene scene;
    scene.sourceLine = node.line;
    scene.transition = story.defaultTransition;
    if (!mapping(node, name.c_str(), sceneIndex))
        return scene;
    if (name == "tutorial") {
        scene.kind = SceneKind::Tutorial;
        rejectUnknown(node, {"subtitle", "actions", "transition"}, "tutorial", sceneIndex);
        if (const Node* value = node.find("subtitle"))
            if (const auto parsed = scalar(*value, "tutorial.subtitle", sceneIndex))
                scene.subtitle = *parsed;
        if (const Node* value = node.find("transition"))
            if (const auto parsed = transition(*value, sceneIndex))
                scene.transition = *parsed;
        const Node* actions = required(node, "actions", "tutorial", sceneIndex);
        if (actions != nullptr) {
            if (actions->type != Node::Type::Sequence)
                issue(*actions, "tutorial.actions must be a sequence", sceneIndex);
            else {
                for (std::size_t i = 0U; i < actions->sequence.size(); ++i) {
                    const Node& item = actions->sequence[i];
                    if (item.type != Node::Type::Mapping) {
                        issue(item, "tutorial action must be a mapping", sceneIndex, i);
                        continue;
                    }
                    if (item.mapping.size() != 1U) {
                        issue(
                            item, "tutorial action must contain exactly one action type",
                            sceneIndex, i);
                        continue;
                    }
                    const auto& actionEntry = item.mapping.front();
                    scene.actions.push_back(
                        decodeAction(actionEntry.first, actionEntry.second, sceneIndex, i));
                }
            }
        }
        return scene;
    }
    rejectUnknown(
        node,
        {"number", "type", "title", "subtitle", "body", "steps", "duration_ms", "transition",
         "narration"},
        name.c_str(), sceneIndex);
    if (name == "chapter")
        scene.kind = SceneKind::Chapter;
    else if (name == "text")
        scene.kind = SceneKind::Text;
    else if (name == "callout")
        scene.kind = SceneKind::Callout;
    else {
        issue(node, "unknown scene type '" + name + "'", sceneIndex);
        return scene;
    }
    if (const Node* value = node.find("number"))
        if (const auto parsed = uintValue(*value, "chapter.number", 1U, 9999U, sceneIndex))
            scene.number = *parsed;
    if (const Node* value = node.find("title"))
        if (const auto parsed = scalar(*value, (name + ".title").c_str(), sceneIndex))
            scene.title = *parsed;
    if (const Node* value = node.find("subtitle"))
        if (const auto parsed = scalar(*value, (name + ".subtitle").c_str(), sceneIndex))
            scene.subtitle = *parsed;
    if (const Node* value = node.find("body"))
        if (const auto parsed = scalar(*value, (name + ".body").c_str(), sceneIndex))
            scene.body = *parsed;
    if (const Node* value = node.find("narration"))
        if (const auto parsed = scalar(*value, (name + ".narration").c_str(), sceneIndex))
            scene.narrationId = *parsed;
    if (const Node* value = required(node, "duration_ms", name.c_str(), sceneIndex))
        if (const auto parsed = uintValue(*value, "duration_ms", 1U, 3600000U, sceneIndex))
            scene.durationMs = *parsed;
    if (const Node* value = node.find("transition"))
        if (const auto parsed = transition(*value, sceneIndex))
            scene.transition = *parsed;
    if (scene.kind == SceneKind::Callout) {
        const Node* type = required(node, "type", "callout", sceneIndex);
        if (type != nullptr) {
            const auto text = scalar(*type, "callout.type", sceneIndex);
            if (text && *text == "tip")
                scene.calloutKind = CalloutKind::Tip;
            else if (text && *text == "warning")
                scene.calloutKind = CalloutKind::Warning;
            else if (text && *text == "recipe")
                scene.calloutKind = CalloutKind::Recipe;
            else if (text)
                issue(*type, "callout.type must be tip, warning or recipe", sceneIndex);
        }
        if (const Node* steps = node.find("steps")) {
            if (steps->type != Node::Type::Sequence)
                issue(*steps, "callout.steps must be a sequence", sceneIndex);
            else
                for (const Node& step : steps->sequence)
                    if (const auto parsed = scalar(step, "callout step", sceneIndex))
                        scene.recipeSteps.push_back(*parsed);
        }
    }
    return scene;
}

StoryParseResult StoryDecoder::decode(const Node& root) {
    if (!mapping(root, "story"))
        return {std::nullopt, std::move(issues)};
    if (const Node* value = root.find("id"))
        if (const auto parsed = scalar(*value, "id"))
            story.id = *parsed;
    rejectUnknown(
        root,
        {"schema", "id", "title", "language", "theme", "interaction_profile", "output",
         "publication", "defaults", "setup", "scenes"},
        "story");
    if (const Node* value = required(root, "schema", "story"))
        if (const auto parsed = uintValue(*value, "schema", 1U, 0xFFFFU))
            story.schema = *parsed;
    if (root.find("id") == nullptr)
        required(root, "id", "story");
    if (const Node* value = required(root, "title", "story"))
        if (const auto parsed = scalar(*value, "title"))
            story.title = *parsed;
    if (const Node* value = required(root, "language", "story"))
        if (const auto parsed = scalar(*value, "language"))
            story.language = *parsed;
    if (const Node* value = required(root, "theme", "story"))
        if (const auto parsed = scalar(*value, "theme"))
            story.theme = *parsed;
    if (const Node* value = required(root, "interaction_profile", "story"))
        if (const auto parsed = profile(*value))
            story.interactionProfile = *parsed;
    if (const Node* value = root.find("output"))
        decodeOutput(*value);
    if (const Node* value = root.find("publication"))
        decodePublication(*value);
    if (const Node* value = root.find("setup"))
        decodeSetup(*value);
    if (const Node* defaults = root.find("defaults")) {
        if (mapping(*defaults, "defaults")) {
            rejectUnknown(*defaults, {"transition"}, "defaults");
            if (const Node* value = defaults->find("transition"))
                if (const auto parsed = transition(*value))
                    story.defaultTransition = *parsed;
        }
    }
    const Node* scenes = required(root, "scenes", "story");
    if (scenes != nullptr) {
        if (scenes->type != Node::Type::Sequence || scenes->sequence.empty())
            issue(*scenes, "scenes must be a non-empty sequence");
        else
            for (std::size_t i = 0U; i < scenes->sequence.size(); ++i) {
                const Node& item = scenes->sequence[i];
                if (item.type != Node::Type::Mapping || item.mapping.size() != 1U)
                    issue(item, "scene entry must contain exactly one scene type", i);
                else
                    story.scenes.push_back(
                        decodeScene(item.mapping.front().first, item.mapping.front().second, i));
            }
    }
    StoryParseResult result;
    result.issues = std::move(issues);
    if (result.issues.empty())
        result.story = std::move(story);
    return result;
}

}  // namespace clockfw::sim::tutorial::detail
