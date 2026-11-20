/**
 * @file tutorial_panel_renderer.h
 * @brief Draws the CLOCK panel, the scaled OLED and the scope trace into tutorial frames.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include "panel_layout.h"
#include "scope_session.h"
#include "simulator_runtime.h"
#include "tutorial/panel_dynamic_layer.h"
#include "tutorial/story_theme.h"
#include "tutorial/tutorial_surface.h"

namespace clockfw::sim::tutorial {

/** @brief Converts a rendered dynamic panel layer into a tutorial surface. */
TutorialSurface dynamicLayerToSurface(const PanelDynamicLayer& layer);

/** @brief Renders the static CLOCK panel with its current OLED image and labels. */
TutorialSurface renderPanelBase(const layout::PanelLayout& layout, const SimulatorRuntime& runtime);

/** @brief Draws the OLED framebuffer into box with the largest integer nearest-neighbour scale. */
void renderOled(
    TutorialSurface& frame,
    const SimulatorRuntime& runtime,
    TutorialRect box,
    const StoryTheme& theme);

/** @brief Scales the panel region source into box (aspect kept, centred) and returns the destination. */
TutorialRect blitPanelRegion(
    TutorialSurface& frame,
    const TutorialSurface& panel,
    TutorialRect source,
    TutorialRect box);

/** @brief Updates the scope session and draws its gate trace into box. */
void renderScope(
    TutorialSurface& frame,
    SimulatorRuntime& runtime,
    scope::Session& session,
    TutorialRect box,
    const StoryTheme& theme);

}  // namespace clockfw::sim::tutorial
