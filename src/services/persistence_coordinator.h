/**
 * @file persistence_coordinator.h
 * @brief Single commit path for every foreground persistence store (D-04).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstdint>

#include "hal/persistent_storage.h"
#include "services/custom_groove_store.h"
#include "services/persistent_state_service.h"
#include "services/sequencer_pattern_store.h"
#include "services/sequencer_step_store.h"

namespace clockfw::services {

/**
 * @brief Decides when the four foreground stores commit and bundles them into one
 * A/B transaction, so one sector erase covers every pending change.
 *
 * Timing: background edits (CURRENT, Sequencer banks) commit after
 * config::kPersistenceCommitDelayMs without a further change, but no later than
 * config::kPersistenceMaximumLatencyMs after the first unsaved change. Explicit
 * user saves (presets, Custom Grooves) and a pending boot migration commit at the
 * next allowed pass. A failed attempt keeps every pending change and retries with
 * exponential backoff from config::kPersistenceRetryInitialMs, at most
 * config::kPersistenceRetryCount times; after the last retry fails, nothing is
 * attempted until the stored data changes again or the module restarts (W-031).
 * A refusal by the commit gate (engine started) is not a failure and is retried
 * without backoff.
 */
class PersistenceCoordinator final {
public:
    /** @brief Persistence state visible to the user interface. */
    enum class Status : std::uint8_t {
        Ok,          ///< Nothing failed since the last successful commit.
        SaveError,   ///< The last attempt failed; pending data is kept and retried.
        SaveLocked,  ///< Flash holds a newer, unknown format; writes are blocked, see F-045.
    };

    /** @brief Binds the coordinator to the shared image and all foreground stores. */
    PersistenceCoordinator(
        hal::PersistentStorage& storage,
        PersistentStateService& state,
        CustomGrooveStore& grooves,
        SequencerPatternStore& patterns,
        SequencerStepStore& steps);

    /**
     * @brief Runs once per foreground pass.
     * @param nowMs Monotonic milliseconds.
     * @param allowFlashWrite Foreground precondition (transport not PLAY); the
     *        storage commit gate still checks the engine atomically before erase.
     * @return True when a new generation was committed in this pass.
     */
    bool service(std::uint32_t nowMs, bool allowFlashWrite);

    /** @brief True while any store holds a change that is not yet durable. */
    bool hasPendingCommit() const;

    /** @brief Current user-visible persistence status. */
    Status status() const { return status_; }

    /** @brief Increments with every event the UI should announce (failure, lock). */
    std::uint32_t noticeRevision() const { return noticeRevision_; }

    /** @brief Consecutive failed or locked attempts since the last success. */
    std::uint8_t consecutiveFailures() const { return consecutiveFailures_; }

    /**
     * @brief True after the first attempt and all config::kPersistenceRetryCount
     * retries failed; no attempt follows until the stored data changes again.
     */
    bool retriesExhausted() const { return retriesExhausted_; }

    /** @brief Delay before the next attempt after @p failures consecutive failures. */
    static std::uint32_t retryDelayMs(std::uint8_t failures);

#ifdef CLOCK_SIMULATOR
    /**
     * @brief Builds the image that a commit at this moment would make durable (simulator hosts).
     *
     * Stages every pending store exactly like a commit, copies the staged image and cancels
     * the update: Flash, pending flags, debounce and retry state stay untouched, so the
     * hardware commit policy (D-04) is unchanged. Without pending work, or while writes are
     * locked, the committed image is returned.
     * @param image Receives the complete logical image.
     * @return False only when the committed image cannot be read.
     */
    bool snapshotImageForSimulator(
        std::array<std::uint8_t, hal::PersistentStorage::kCapacityBytes>& image);

    /** @brief Changes whenever the snapshot may differ: store revisions, pending state, Flash. */
    std::uint64_t snapshotRevisionForSimulator() const;
#endif

private:
    /** @brief Sum of all store change revisions; any difference restarts the quiet timer. */
    std::uint32_t combinedRevision() const;

    /** @brief combinedRevision() plus Custom Groove mutations; any difference ends a retry pause. */
    std::uint32_t storedDataRevision() const;

    /** @brief True for pending work that bypasses the debounce. */
    bool hasUrgentCommit() const;

    /** @brief Applies backoff and status after an unsuccessful attempt. */
    void recordFailure(std::uint32_t nowMs, bool locked);

    /** @brief Stages every store into one open update and commits it. */
    bool commitAll(std::uint32_t nowMs);

    hal::PersistentStorage& storage_;
    PersistentStateService& state_;
    CustomGrooveStore& grooves_;
    SequencerPatternStore& patterns_;
    SequencerStepStore& steps_;
    std::uint32_t firstChangeMs_ = 0U;
    std::uint32_t lastChangeMs_ = 0U;
    std::uint32_t lastRevision_ = 0U;
    std::uint32_t retryAtMs_ = 0U;
    std::uint32_t noticeRevision_ = 0U;
    std::uint32_t exhaustedRevision_ = 0U;
    Status status_ = Status::Ok;
    std::uint8_t consecutiveFailures_ = 0U;
    bool debounceActive_ = false;
    bool retryScheduled_ = false;
    bool retriesExhausted_ = false;
};

}  // namespace clockfw::services
