/**
 * @file storybook_parser_action_tests.cpp
 * @brief Storybook parser regression cases for button state, push hold, focus and text blocks.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "storybook_parser_action_tests.h"

#include "tutorial/story_parser.h"
#include "tutorial/story_validator.h"

namespace clockfw::sim::tutorial::parser_tests {

bool actionAndTextCasesPass(const std::filesystem::path& tutorialRoot) {
    bool ok = true;
    const StoryParseResult heldTap = parseStoryText(R"YAML(
schema: 1
id: held-tap
title: "Held TAP"
language: en
theme: south-signal-lab-default
interaction_profile: HUMAN_NORMAL
scenes:
  - tutorial:
      actions:
        - button:
            name: TAP
            state: down
        - encoder_push: {}
        - button:
            name: TAP
            state: up
)YAML");
    ok &= require(
        heldTap && validateStory(*heldTap.story, tutorialRoot).empty(),
        "explicit button down/up must validate for real modifier gestures");
    ok &= require(
        heldTap && heldTap.story->scenes[0].actions[0].buttonState == true &&
            heldTap.story->scenes[0].actions[2].buttonState == false,
        "button down/up states must decode explicitly");

    const StoryParseResult longPush = parseStoryText(R"YAML(
schema: 1
id: long-encoder-push
title: "Long push"
language: en
theme: south-signal-lab-default
interaction_profile: HUMAN_NORMAL
scenes:
  - tutorial:
      actions:
        - encoder_push:
            hold_ms: 800
)YAML");
    ok &= require(
        longPush && longPush.story->scenes[0].actions[0].holdMs == 800U &&
            validateStory(*longPush.story, tutorialRoot).empty(),
        "encoder_push.hold_ms must preserve a real long-push duration");

    const StoryParseResult focusAction = parseStoryText(R"YAML(
schema: 1
id: focus-action
title: "Focus action"
language: en
theme: south-signal-lab-default
interaction_profile: HUMAN_NORMAL
scenes:
  - tutorial:
      actions:
        - focus:
            target: oled_region
            x: 0
            y: 0
            width: 64
            height: 12
            placement: above
            label: "Top bar"
            duration_ms: 2400
)YAML");
    ok &= require(
        focusAction &&
            focusAction.story->scenes[0].actions[0].focusTarget == FocusTarget::OledRegion &&
            focusAction.story->scenes[0].actions[0].focusWidth == 64 &&
            focusAction.story->scenes[0].actions[0].durationMs == 2400U &&
            focusAction.story->scenes[0].actions[0].focusPlacement == FocusPlacement::Above &&
            validateStory(*focusAction.story, tutorialRoot).empty(),
        "presentation-only focus action must decode and validate OLED regions");

    const StoryParseResult orphanRelease = parseStoryText(R"YAML(
schema: 1
id: orphan-button-release
title: "Orphan release"
language: en
theme: south-signal-lab-default
interaction_profile: HUMAN_NORMAL
scenes:
  - tutorial:
      actions:
        - button:
            name: TAP
            state: up
)YAML");
    ok &= require(
        orphanRelease &&
            containsReason(validateStory(*orphanRelease.story, tutorialRoot), "held down"),
        "button release without matching hold must fail semantic validation");

    const StoryParseResult badButtonState = parseStoryText(R"YAML(
schema: 1
id: bad-button-state
title: "Bad button state"
language: en
theme: south-signal-lab-default
interaction_profile: HUMAN_NORMAL
scenes:
  - tutorial:
      actions:
        - button:
            name: TAP
            state: toggle
)YAML");
    ok &= require(
        !badButtonState && containsReason(badButtonState.issues, "button.state"),
        "unknown explicit button state must fail parsing");

    const StoryParseResult blockText = parseStoryText(R"YAML(
schema: 1
id: block-text
title: "Block text"
language: en
theme: south-signal-lab-default
interaction_profile: HUMAN_NORMAL
scenes:
  - text:
      title: "Why AUTO"
      body: |
        AUTO keeps the internal clock available.
        External lock does not rewrite SOURCE.
      duration_ms: 2500
)YAML");
    ok &= require(
        blockText &&
            blockText.story->scenes[0].body ==
                "AUTO keeps the internal clock available.\nExternal lock does not rewrite SOURCE.",
        "literal block text must decode without clipping or indentation artifacts");

    const StoryParseResult unsupportedYaml = parseStoryText(R"YAML(
schema: 1
id: unsupported-yaml
title: "Unsupported YAML"
language: en
theme: south-signal-lab-default
interaction_profile: HUMAN_NORMAL
meta: [one, two]
scenes: []
)YAML");
    ok &= require(
        !unsupportedYaml && containsReason(unsupportedYaml.issues, "flow collections"),
        "unsupported YAML features must fail rather than silently degrade");
    return ok;
}

}  // namespace clockfw::sim::tutorial::parser_tests
