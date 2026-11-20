/**
 * @file groove_editor_renderer.h
 * @brief Compact 128x64 Custom Groove editor and library rendering.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include "hal/oled_display.h"
#include "services/custom_groove_store.h"
#include "ui/ui_types.h"

namespace clockfw::ui {

/** @brief Renders the graphical Custom Groove editor on the production framebuffer. */
class GrooveEditorRenderer final {
  public:
    /**
     * @brief Constructs the renderer around the OLED HAL.
     * @param display Display that receives the drawing commands.
     */
    explicit GrooveEditorRenderer(hal::OledDisplay& display);

    /**
     * @brief Renders the current draft with beat/step grid and signed microtiming markers.
     * @param navigation Groove draft, cursor and zoom state.
     */
    void renderEditor(const NavigationState& navigation);

    /**
     * @brief Renders the TAP recorder with captured markers, playhead, and Pre-Count status.
     * @param navigation Groove draft and recorder state.
     */
    void renderRecorder(const NavigationState& navigation);

    /**
     * @brief Renders the ten-slot fixed-record Custom Groove library.
     * @param navigation Slot cursor, scroll state and slot action.
     * @param store Custom Groove library that supplies the slot names.
     */
    void renderSlots(const NavigationState& navigation, const services::CustomGrooveStore& store);

    /**
     * @brief Renders overwrite/discard confirmation screens.
     * @param title NUL-terminated question shown as the title.
     * @param navigation NO/YES cursor.
     */
    void renderConfirm(const char* title, const NavigationState& navigation);

    /**
     * @brief Renders the Custom Groove name editor.
     * @param navigation Edited name and character position.
     */
    void renderNameEntry(const NavigationState& navigation);

  private:
    /** Display that receives the drawing commands. */
    hal::OledDisplay& display_;
};

}  // namespace clockfw::ui
