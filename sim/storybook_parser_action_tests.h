/**
 * @file storybook_parser_action_tests.h
 * @brief Shared checks of the Storybook parser tests and the button, focus and text-block cases.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "tutorial/story_model.h"

namespace clockfw::sim::tutorial::parser_tests {

/** @brief Reports message on stderr when condition is false; returns condition. */
bool require(bool condition, const std::string& message);

/** @brief True when one of issues has a reason containing needle. */
bool containsReason(const std::vector<StoryIssue>& issues, const std::string& needle);

/** @brief Runs the button-state, push-hold, focus and text-block cases; true when all pass. */
bool actionAndTextCasesPass(const std::filesystem::path& tutorialRoot);

}  // namespace clockfw::sim::tutorial::parser_tests
