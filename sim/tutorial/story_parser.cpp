/**
 * @file story_parser.cpp
 * @brief Decodes the strict CLOCK Storybook YAML subset into the typed schema-1 model.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "tutorial/story_parser.h"
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
namespace clockfw::sim::tutorial {
StoryParseResult::operator bool() const {
    return story.has_value() && issues.empty();
}
StoryParseResult parseStoryText(const std::string_view source) {
    const StoryYamlResult yaml = parseStoryYaml(source);
    if (!yaml) {
        StoryParseResult result;
        for (const auto& issue : yaml.issues)
            result.issues.push_back({{}, std::nullopt, std::nullopt, issue.line, issue.reason});
        return result;
    }
    detail::StoryDecoder decoder;
    return decoder.decode(*yaml.root);
}
StoryParseResult parseStoryFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        StoryParseResult result;
        result.issues.push_back(
            {{}, std::nullopt, std::nullopt, 0U, "cannot open story file: " + path.string()});
        return result;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parseStoryText(buffer.str());
}
}  // namespace clockfw::sim::tutorial
