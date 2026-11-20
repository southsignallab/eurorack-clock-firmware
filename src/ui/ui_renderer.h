/**
 * @file ui_renderer.h
 * @brief Top-level UI rendering dispatcher and boot-screen renderer.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

#include "domain/clock_types.h"
#include "engine/clock_engine.h"
#include "hal/oled_display.h"
#include "services/custom_groove_store.h"
#include "services/persistent_state_service.h"
#include "services/sequencer_pattern_store.h"
#include "services/sequencer_step_store.h"
#include "ui/channel_navigation_renderer.h"
#include "ui/groove_editor_renderer.h"
#include "ui/performance_renderer.h"
#include "ui/screensaver_renderer.h"
#include "ui/settings_renderer.h"
#include "ui/ui_types.h"

namespace clockfw::ui {

/**
 * @brief Coordinates screen-specific renderers without owning navigation or application logic.
 *
 * Keeping this class as a small dispatcher prevents screen drawing, input handling, and
 * musical state mutation from accumulating in one monolithic UI object.
 */
class UiRenderer final {
  public:
    /**
     * @brief Constructs all screen renderers around one OLED HAL instance.
     * @param display Display abstraction receiving the actual drawing commands.
     * @param persistentState Source of the named-preset slot names and occupancy.
     */
    UiRenderer(hal::OledDisplay& display, const services::PersistentStateService& persistentState);

    /**
     * @brief Constructs the renderer with access to the persistent Custom Groove library.
     * @param display Display abstraction receiving the actual drawing commands.
     * @param persistentState Source of the named-preset slot names and occupancy.
     * @param customGrooveStore Custom Groove library shown by the Groove screens.
     */
    UiRenderer(
        hal::OledDisplay& display,
        const services::PersistentStateService& persistentState,
        const services::CustomGrooveStore& customGrooveStore);

    /**
     * @brief Constructs the renderer with Custom Groove and Sequencer 2.0 stores.
     * @param display Display abstraction receiving the actual drawing commands.
     * @param persistentState Source of the named-preset slot names and occupancy.
     * @param customGrooveStore Custom Groove library shown by the Groove screens.
     * @param sequencerPatternStore Sequencer 2.0 patterns shown by the Sequencer screens.
     */
    UiRenderer(
        hal::OledDisplay& display,
        const services::PersistentStateService& persistentState,
        const services::CustomGrooveStore& customGrooveStore,
        const services::SequencerPatternStore& sequencerPatternStore);

    /**
     * @brief Constructs the renderer with persistent Sequencer step expression.
     * @param display Display abstraction receiving the actual drawing commands.
     * @param persistentState Source of the named-preset slot names and occupancy.
     * @param customGrooveStore Custom Groove library shown by the Groove screens.
     * @param sequencerPatternStore Sequencer 2.0 patterns shown by the Sequencer screens.
     * @param sequencerStepStore Sequencer 2.0 step expression shown by the step editor.
     */
    UiRenderer(
        hal::OledDisplay& display,
        const services::PersistentStateService& persistentState,
        const services::CustomGrooveStore& customGrooveStore,
        const services::SequencerPatternStore& sequencerPatternStore,
        const services::SequencerStepStore& sequencerStepStore);

    /**
     * @brief Renders the screen selected by the current navigation state.
     * @param state Complete musical/application configuration.
     * @param navigation Current UI navigation state.
     * @param engineSnapshot Read-only real-time timing snapshot.
     * @param diagnostics Live input/output levels for the hardware diagnostics pages.
     */
    void render(
        const ClockState& state,
        const NavigationState& navigation,
        const engine::EngineSnapshot& engineSnapshot,
        const DiagnosticSnapshot& diagnostics = {});

    /**
     * @brief Renders the one-second boot screen using the generated 1-bit CLOCK logo bitmap and
     *        progress bar.
     * @param elapsedMs Milliseconds elapsed in the boot-screen interval.
     */
    void renderBootScreen(std::uint32_t elapsedMs);

    /**
     * @brief Renders one STOP-mode screensaver frame.
     * @param mode Configured screensaver mode.
     * @param frameIndex Monotonic animation frame number.
     */
    void renderScreensaver(ScreensaverMode mode, std::uint32_t frameIndex);

    /**
     * @brief Applies normal or dimmed OLED contrast.
     * @param dimmed true selects the dimmed contrast.
     */
    void setDisplayDimmed(bool dimmed);

    /**
     * @brief Switches the OLED panel on or off without clearing display RAM.
     * @param enabled true switches the panel on.
     */
    void setDisplayPower(bool enabled);

    /**
     * @brief Reports whether one read-only settings value is abbreviated on screen.
     * @param label Row label; nullptr yields false.
     * @param value Row value; nullptr or an empty string yields false.
     * @return true when the value does not fit next to the label in the small font.
     */
    bool informationValueOverflows(const char* label, const char* value);

  private:
    /** Display shared by all screen renderers. */
    hal::OledDisplay& display_;
    /** Source of the named-preset slot names and occupancy. */
    const services::PersistentStateService& persistentState_;
    /** Optional Custom Groove library. */
    const services::CustomGrooveStore* customGrooveStore_ = nullptr;
    /** Optional Sequencer 2.0 pattern store. */
    const services::SequencerPatternStore* sequencerPatternStore_ = nullptr;
    /** Optional Sequencer 2.0 step expression store. */
    const services::SequencerStepStore* sequencerStepStore_ = nullptr;
    /** Performance screen renderer. */
    PerformanceRenderer performanceRenderer_;
    /** Channel quick select, mode select/confirm and Sequencer editor renderer. */
    ChannelNavigationRenderer channelNavigationRenderer_;
    /** Settings, preset and confirmation screen renderer. */
    SettingsRenderer settingsRenderer_;
    /** Custom Groove editor, recorder and dialog renderer. */
    GrooveEditorRenderer grooveEditorRenderer_;
    /** STOP-mode screensaver renderer. */
    ScreensaverRenderer screensaverRenderer_;
};

}  // namespace clockfw::ui
