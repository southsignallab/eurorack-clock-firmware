/**
 * @file story_text_system_font.h
 * @brief Host system-font backend of the Storybook text renderer (Windows GDI; elsewhere unavailable).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <string>

#include "tutorial/story_theme.h"
#include "tutorial/tutorial_surface.h"

namespace clockfw::sim::tutorial {

/** @brief Width of text in the system font, in pixels; throws when the font is unavailable. */
int systemTextWidth(const std::string& text, const StoryFontStyle& font);

/** @brief Line height of the system font, in pixels; throws when the font is unavailable. */
int systemFontHeight(const StoryFontStyle& font);

/** @brief Blends one line of text at (x, y) and returns its advance width in pixels. */
int drawSystemTextLine(
    TutorialSurface& surface,
    const std::string& text,
    const StoryFontStyle& font,
    int x,
    int y,
    TutorialColor color);

}  // namespace clockfw::sim::tutorial
