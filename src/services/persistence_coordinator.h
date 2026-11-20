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

    /**
     * @brief Binds the coordinator to the shared image and all foreground stores.
     * @param storage Shared persistent image; not owned.
     * @param state CURRENT and preset store; not owned.
     * @param grooves Custom Groove store; not owned.
     * @param patterns Sequencer pattern store; not owned.
     * @param steps Sequencer step store; not owned.
     */
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
    [[nodiscard]] bool service(std::uint32_t nowMs, bool allowFlashWrite);

    /**
     * @brief Reports changes that are not yet durable.
     * @return True while any store holds a change that is not yet durable.
     */
    bool hasPendingCommit() const;

    /**
     * @brief Current user-visible persistence status.
     * @return Status of the last attempt or of the write lock.
     */
    Status status() const {
        return status_;
    }

    /**
     * @brief Revision of events the UI should announce.
     * @return Counter that increments with every failure or newly detected lock.
     */
    std::uint32_t noticeRevision() const {
        return noticeRevision_;
    }

    /**
     * @brief Consecutive failed or locked attempts since the last success.
     * @return Failure count, bounded by config::kPersistenceRetryCount + 1.
     */
    std::uint8_t consecutiveFailures() const {
        return consecutiveFailures_;
    }

    /**
     * @brief True after the first attempt and all config::kPersistenceRetryCount
     * retries failed; no attempt follows until the stored data changes again.
     * @return True while retries are exhausted.
     */
    bool retriesExhausted() const {
        return retriesExhausted_;
    }

    /**
     * @brief Delay before the next attempt after @p failures consecutive failures.
     * @param failures Consecutive failures so far (1 = first failure).
     * @return Delay in milliseconds: config::kPersistenceRetryInitialMs doubled per further
     *         failure, capped at config::kPersistenceRetryMaximumMs.
     */
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
    /**
     * @brief Sum of the CURRENT, Sequencer pattern and Sequencer step change revisions.
     *
     * Any difference restarts the quiet timer. Custom Groove edits are urgent and
     * therefore not part of this sum.
     * @return Wrapping sum of the three revisions.
     */
    std::uint32_t combinedRevision() const;

    /**
     * @brief combinedRevision() plus Custom Groove mutations.
     * @return Wrapping sum; any difference ends a retry pause after exhausted retries.
     */
    std::uint32_t storedDataRevision() const;

    /**
     * @brief Reports pending work that bypasses the debounce.
     * @return True for preset edits, a pending boot migration or Custom Groove edits.
     */
    bool hasUrgentCommit() const;

    /**
     * @brief Applies backoff and status after an unsuccessful attempt.
     * @param nowMs Current monotonic time in milliseconds.
     * @param locked True when the attempt failed because writes are locked (SaveLocked).
     */
    void recordFailure(std::uint32_t nowMs, bool locked);

    /**
     * @brief Stages every store into one open update and commits it.
     * @param nowMs Current monotonic time in milliseconds, used for the retry schedule.
     * @return True only when a new generation was committed; false on failure, on a
     *         deferral by the commit gate, or when the commit wrote nothing new.
     */
    [[nodiscard]] bool commitAll(std::uint32_t nowMs);

    /** Shared persistent image; not owned. */
    hal::PersistentStorage& storage_;
    /** CURRENT and preset store; not owned. */
    PersistentStateService& state_;
    /** Custom Groove store; not owned. */
    CustomGrooveStore& grooves_;
    /** Sequencer pattern store; not owned. */
    SequencerPatternStore& patterns_;
    /** Sequencer step store; not owned. */
    SequencerStepStore& steps_;
    /** Time in ms of the first unsaved change of the current debounce window. */
    std::uint32_t firstChangeMs_ = 0U;
    /** Time in ms of the latest observed revision change. */
    std::uint32_t lastChangeMs_ = 0U;
    /** combinedRevision() seen at lastChangeMs_. */
    std::uint32_t lastRevision_ = 0U;
    /** Earliest time in ms of the next attempt after a failure. */
    std::uint32_t retryAtMs_ = 0U;
    /** Value returned by noticeRevision(). */
    std::uint32_t noticeRevision_ = 0U;
    /** storedDataRevision() when retries were exhausted. */
    std::uint32_t exhaustedRevision_ = 0U;
    /** Value returned by status(). */
    Status status_ = Status::Ok;
    /** Value returned by consecutiveFailures(). */
    std::uint8_t consecutiveFailures_ = 0U;
    /** True while a debounce window is open. */
    bool debounceActive_ = false;
    /** True while retryAtMs_ gates the next attempt. */
    bool retryScheduled_ = false;
    /** Value returned by retriesExhausted(). */
    bool retriesExhausted_ = false;
};

}  // namespace clockfw::services
