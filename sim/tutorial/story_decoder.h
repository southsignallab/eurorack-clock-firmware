/**
 * @file story_decoder.h
 * @brief Typed decoder of the CLOCK Storybook YAML subset (schema 1) with per-field issues.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "tutorial/story_parser.h"
#include "tutorial/story_yaml.h"

namespace clockfw::sim::tutorial::detail {

/** YAML node type the decoder reads. */
using Node = StoryYamlNode;

/** @brief Decodes one parsed YAML document into a Story and collects every schema issue. */
struct StoryDecoder final {
    Story story;
    std::vector<StoryIssue> issues;
    void issue(
        const Node& node,
        const std::string& reason,
        std::optional<std::size_t> scene = std::nullopt,
        std::optional<std::size_t> action = std::nullopt);
    bool mapping(
        const Node& node,
        const char* context,
        std::optional<std::size_t> scene = std::nullopt,
        std::optional<std::size_t> action = std::nullopt);
    void rejectUnknown(
        const Node& node,
        std::initializer_list<const char*> allowed,
        const char* context,
        std::optional<std::size_t> scene = std::nullopt,
        std::optional<std::size_t> action = std::nullopt);
    const Node* required(
        const Node& node,
        const char* key,
        const char* context,
        std::optional<std::size_t> scene = std::nullopt,
        std::optional<std::size_t> action = std::nullopt);
    std::optional<std::string> scalar(
        const Node& node,
        const char* context,
        std::optional<std::size_t> scene = std::nullopt,
        std::optional<std::size_t> action = std::nullopt);
    std::optional<std::uint32_t> uintValue(
        const Node& node,
        const char* context,
        const std::uint32_t min,
        const std::uint32_t max,
        std::optional<std::size_t> scene = std::nullopt,
        std::optional<std::size_t> action = std::nullopt);
    std::optional<bool> boolValue(
        const Node& node,
        const char* context,
        std::optional<std::size_t> scene = std::nullopt,
        std::optional<std::size_t> action = std::nullopt);
    std::optional<TransitionKind> transition(
        const Node& node,
        std::optional<std::size_t> scene = std::nullopt);
    std::optional<InteractionProfile> profile(const Node& node);
    std::optional<StoryWaveform> waveform(const Node& node, std::size_t scene, std::size_t action);
    void decodeOutput(const Node& node);
    void decodePublication(const Node& node);
    void decodeSetup(const Node& node);
    StoryAction decodeAction(
        const std::string& name,
        const Node& node,
        const std::size_t sceneIndex,
        const std::size_t actionIndex);
    StoryScene decodeScene(const std::string& name, const Node& node, const std::size_t sceneIndex);
    StoryParseResult decode(const Node& root);
};

}  // namespace clockfw::sim::tutorial::detail
