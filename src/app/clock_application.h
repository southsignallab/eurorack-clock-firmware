/**
 * @file clock_application.h
 * @brief Top-level composition root and lifecycle coordinator for the firmware.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include "domain/clock_types.h"
#include "engine/clock_engine.h"
#include "game/arcade_leaderboard_store.h"
#include "game/beatknecht.h"
#include "game/breakout_game.h"
#include "game/egg_journey_game.h"
#include "game/formula1_game.h"
#include "game/pixel_raid_game.h"
#include "hal/control_panel.h"
#include "hal/external_input_capture.h"
#include "hal/gate_output_driver.h"
#include "hal/oled_display.h"
#include "hal/periodic_timer.h"
#include "hal/persistent_storage.h"
#include "services/custom_groove_store.h"
#include "services/external_sync_controller.h"
#include "services/persistence_coordinator.h"
#include "services/persistent_state_service.h"
#include "services/sequencer_pattern_store.h"
#include "services/sequencer_step_store.h"
#include "ui/ui_controller.h"
#include "ui/ui_renderer.h"

namespace clockfw::app {

/**
 * @brief Wires together HAL, real-time engine, persistence, domain state, and UI components.
 *
 * ClockApplication intentionally contains lifecycle orchestration only. Hardware
 * details remain in HAL classes, timing behavior in ClockEngine, persistence in
 * PersistentStateService, and interaction behavior in UiController.
 */
class ClockApplication final {
  public:
    /** @brief Constructs all firmware components in dependency order. */
    ClockApplication();

    /**
     * @brief Initializes factory defaults, persisted metadata, hardware, boot UI, and
     *        scheduler safely.
     */
    void begin();

    /**
     * @brief Initializes the application from an explicit state snapshot.
     * @param initialState State to activate after safe hardware initialization.
     *
     * The supplied transport value is deliberately ignored for startup execution:
     * every boot enters STOP, even when the persisted or supplied state was PLAY.
     */
    void begin(const ClockState& initialState);

    /** @brief Executes one non-real-time application iteration. */
    void runOnce();

#ifdef CLOCK_HOST_TEST
    /** @brief Returns the persistence coordinator for host integration tests. */
    const services::PersistenceCoordinator& persistenceForTest() const {
        return persistenceCoordinator_;
    }

    /** @brief Returns UI navigation state for host integration tests. */
    const ui::NavigationState& navigationForTest() const {
        return uiController_.navigation();
    }

    /** @brief Returns the foreground transport field for host regression tests. */
    TransportState transportForTest() const;

    /** @brief Returns whether the real-time engine is running, for host regression tests. */
    bool enginePlayingForTest() const;

    /** @brief Returns the foreground internal/fallback BPM for host regression tests. */
    std::uint16_t bpmForTest() const;

    /** @brief True while STOP-mode display protection has switched the OLED off. */
    bool displayPoweredOffForTest() const {
        return uiController_.displayPoweredOffForTest();
    }
#endif

#ifdef CLOCK_SIMULATOR
    /** @brief Returns read-only current application state for simulator instrumentation. */
    const ClockState& stateForSimulator() const;

    /** @brief Returns read-only OLED HAL instance exposing the real firmware framebuffer. */
    const hal::OledDisplay& displayForSimulator() const;

    /** @brief Returns current timing-engine snapshot for simulator diagnostics. */
    engine::EngineSnapshot engineSnapshotForSimulator() const;

    /** @brief Injects one already-conditioned SYNC comparator level transition. */
    void injectExternalSyncLevelForSimulator(bool high);

    /** @brief Clears the simulator-provided external-sync lock after a cable/signal loss. */
    void clearExternalSyncForSimulator();

    /** @brief Injects one already-conditioned RST comparator level transition. */
    void injectExternalResetLevelForSimulator(bool high);

    /** @brief Returns true once boot/game handling has completed and the scheduler is active. */
    bool runningForSimulator() const;

    /** @brief Returns true while the configured boot Easter egg owns display/controls. */
    bool easterEggActiveForSimulator() const;

    /** @brief Returns the logical gate-output enable state used by the simulator. */
    bool gateOutputEnabledForSimulator() const;

    /** @brief Places the simulated module into its electrically safe powered-off state. */
    void powerOffForSimulator();

    /** @brief Opens the production General Settings page from a desktop host shell. */
    bool openGeneralSettingsForSimulator();

    /** @brief Returns current UI navigation state for host integration tests. */
    const ui::NavigationState& navigationForSimulator() const;

    /**
     * @brief Builds the persistence image including accepted pending operations (host patches).
     * @param image Receives the complete logical image.
     * @return False only when the committed image cannot be read.
     */
    bool persistenceSnapshotForSimulator(
        std::array<std::uint8_t, hal::PersistentStorage::kCapacityBytes>& image);

    /** @brief Changes whenever persistenceSnapshotForSimulator() may return different bytes. */
    std::uint64_t persistenceSnapshotRevisionForSimulator() const;
#endif

  private:
    /**
     * @brief Timer ISR thunk used by the HAL periodic-timer callback boundary.
     *
     * Opens the engine quantum, runs the sync service, then the engine tick over the
     * elapsed ticks, which ends with the single gate write.
     *
     * @isr TIM3 scheduler interrupt: bounded (catch-up limited to
     *      config::kMaximumSchedulerCatchUpTicks), non-blocking, no allocation, opens no
     *      interrupt lock.
     */
    static void schedulerInterruptThunk();

    /**
     * @brief Applies the latest ISR transport transition if it still matches the engine.
     * @param nowMs Current monotonic time; input transport commands are reported as activity.
     */
    void adoptExternalTransportTransition(std::uint32_t nowMs);

    /**
     * @brief Returns true only when a Flash sector commit cannot stall a running engine.
     *
     * Foreground precondition of the persistence coordinator. The storage commit
     * gate (ExternalSyncController) re-checks the engine atomically before erase.
     *
     * @return True when neither the foreground transport nor the engine is playing.
     */
    bool flashWriteAllowed() const;

    /**
     * @brief Runs the persistence coordinator and forwards new failures to the UI.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void servicePersistence(std::uint32_t nowMs);

    /**
     * @brief Renders the boot screen and returns true only when encoder push stayed held
     *        throughout.
     * @return True when the encoder button was pressed for the whole boot duration.
     *
     * Blocks for config::kBootDurationMs with the output stage disabled and all gates LOW.
     */
    bool runBootSequence();

    /** @brief Runs the compile-time selected boot Easter egg synchronously. */
    void runSelectedEasterEgg();

#ifdef CLOCK_SIMULATOR
    /** @brief Starts the compile-time selected boot Easter egg in non-blocking simulator mode. */
    void beginSelectedEasterEggForSimulator();

    /** @brief Services the active simulator Easter egg; false means it has exited. */
    bool serviceSelectedEasterEggForSimulator(std::uint32_t nowMs);
#endif

    /** @brief Loads the fixed-record Custom Groove library into the real-time engine cache. */
    void loadCustomGrooveLibrary();

    /** @brief Loads every channel's persisted active Sequencer 2.0 pattern into the engine. */
    void loadSequencerPatternBank();

    /**
     * @brief Applies persistent device-local control/display preferences to the HAL only when
     *        they change.
     * @param force True applies both preferences even when they did not change.
     */
    void synchronizeDevicePreferences(bool force = false);

    /** @brief Enters a safe permanent failure state when the display cannot be initialized. */
    [[noreturn]] void haltSafely();

#ifdef CLOCK_SIMULATOR
    /** @brief Services the non-blocking boot/game lifecycle used only by the native simulator. */
    void serviceSimulatorStartup(std::uint32_t nowMs);

    /** @brief Completes simulator startup and enables the real scheduler/output stage. */
    void finishSimulatorStartup();
#endif

    /** Active composition-root instance used only by the static timer ISR thunk. */
    static ClockApplication* activeInstance_;

    /** Application state edited by the UI; foreground only. */
    ClockState state_{};
    /** Driver of the eight gate outputs. */
    hal::GateOutputDriver gateOutputs_{};
    /** Encoder and button input. */
    hal::ControlPanel controlPanel_{};
    /** Edge capture of the two comparator inputs. */
    hal::ExternalInputCapture externalInputs_{};
    /** OLED display. */
    hal::OledDisplay display_{};
    /** TIM3 scheduler timer. */
    hal::PeriodicTimer schedulerTimer_{};
    /** Internal Flash storage shared by every store and game. */
    hal::PersistentStorage persistentStorage_{};
    /** Pixel Raid high scores. */
    game::ArcadeLeaderboardStore pixelRaidLeaderboard_;
    /** Formula 1 high scores. */
    game::ArcadeLeaderboardStore formula1Leaderboard_;
    /** Breakout high scores. */
    game::ArcadeLeaderboardStore breakoutLeaderboard_;
    /** Egg Journey high scores. */
    game::ArcadeLeaderboardStore eggJourneyLeaderboard_;
    /** Leaderboard of the compile-time selected game; nullptr when it has none. */
    game::ArcadeLeaderboardStore* selectedLeaderboard_ = nullptr;
    /** Pixel Raid boot game. */
    game::PixelRaidGame pixelRaidGame_;
    /** Formula 1 boot game. */
    game::Formula1Game formula1Game_;
    /** Breakout boot game. */
    game::BreakoutGame breakoutGame_;
    /** Egg Journey boot game. */
    game::EggJourneyGame eggJourneyGame_;
    /** Beatknecht boot game. */
    game::Beatknecht beatknecht_;
    /** Persistent ClockState store. */
    services::PersistentStateService persistentState_;
    /** Persistent Custom Groove library. */
    services::CustomGrooveStore customGrooveStore_;
    /** Persistent Sequencer 2.0 patterns. */
    services::SequencerPatternStore sequencerPatternStore_;
    /** Persistent Sequencer 2.0 step expressions. */
    services::SequencerStepStore sequencerStepStore_;
    /** Real-time scheduler engine. */
    engine::ClockEngine engine_;
    /** External sync and input service; also the Flash commit gate (D-04). */
    services::ExternalSyncController externalSyncController_;
    /** Schedules and retries the Flash commits of every store. */
    services::PersistenceCoordinator persistenceCoordinator_;
    /** Persistence notice revision already shown by the UI. */
    std::uint32_t announcedPersistenceNotice_ = 0U;
    /** Input transport-command count already reported to the UI as activity (E-11). */
    std::uint32_t seenInputTransportCommands_ = 0U;
    /** Control-activity revision seen by the last persistence pass (W-311 reminder). */
    std::uint32_t remindedControlActivity_ = 0U;
    /** Screen renderer. */
    ui::UiRenderer renderer_;
    /** Front-panel interaction controller. */
    ui::UiController uiController_;
    /** Encoder direction last applied to the control panel. */
    bool appliedEncoderDirectionReversed_ = false;
    /** Display rotation last applied to the display. */
    bool appliedDisplayRotated180_ = false;
    /** True once synchronizeDevicePreferences() has applied both preferences. */
    bool devicePreferencesApplied_ = false;
#ifdef CLOCK_SIMULATOR
    enum class SimulatorLifecycle : std::uint8_t { Booting, EasterEgg, Running, PoweredOff };
    SimulatorLifecycle simulatorLifecycle_ = SimulatorLifecycle::PoweredOff;
    std::uint32_t simulatorBootStartedAtMs_ = 0U;
    std::uint32_t simulatorLastBootRenderAtMs_ = UINT32_MAX;
    bool simulatorEncoderHeldForEntireBoot_ = false;
#endif
};

}  // namespace clockfw::app
