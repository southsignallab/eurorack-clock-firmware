/**
 * @file persistence_coordinator.cpp
 * @brief Central debounce, single-transaction commit, and backoff for foreground persistence (D-04).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "services/persistence_coordinator.h"

#include "config.h"

namespace clockfw::services {

PersistenceCoordinator::PersistenceCoordinator(
    hal::PersistentStorage& storage,
    PersistentStateService& state,
    CustomGrooveStore& grooves,
    SequencerPatternStore& patterns,
    SequencerStepStore& steps)
    : storage_(storage), state_(state), grooves_(grooves), patterns_(patterns), steps_(steps) {}

std::uint32_t PersistenceCoordinator::retryDelayMs(const std::uint8_t failures) {
    std::uint32_t delayMs = config::kPersistenceRetryInitialMs;
    for (std::uint8_t step = 1U; step < failures && delayMs < config::kPersistenceRetryMaximumMs; ++step) {
        delayMs *= 2U;
    }
    return delayMs < config::kPersistenceRetryMaximumMs ? delayMs : config::kPersistenceRetryMaximumMs;
}

bool PersistenceCoordinator::hasPendingCommit() const {
    return state_.hasPendingCommit() || grooves_.hasPendingCommit() || patterns_.dirty() || steps_.dirty();
}

bool PersistenceCoordinator::hasUrgentCommit() const {
    return state_.hasUrgentCommit() || grooves_.hasPendingCommit();
}

std::uint32_t PersistenceCoordinator::combinedRevision() const {
    return state_.changeRevision() + patterns_.changeRevision() + steps_.changeRevision();
}

void PersistenceCoordinator::recordFailure(const std::uint32_t nowMs, const bool locked) {
    if (consecutiveFailures_ < UINT8_MAX) {
        ++consecutiveFailures_;
    }
    retryAtMs_ = nowMs + retryDelayMs(consecutiveFailures_);
    retryScheduled_ = true;
    status_ = locked ? Status::SaveLocked : Status::SaveError;
    ++noticeRevision_;
}

bool PersistenceCoordinator::commitAll(const std::uint32_t nowMs) {
    if (!storage_.beginUpdate()) {
        recordFailure(nowMs, storage_.writeLocked());
        return false;
    }
    // One update for all stores: one erase instead of up to four, and no
    // durable CURRENT reference to a Custom Groove slot that is not yet written.
    const bool staged =
        grooves_.stage() && state_.stagePendingRecords(true) && patterns_.stage() && steps_.stage();
    if (!staged) {
        storage_.cancelUpdate();
        recordFailure(nowMs, false);
        return false;
    }
    if (!storage_.commitUpdate()) {
        const hal::CommitOutcome outcome = storage_.lastCommitOutcome();
        if (outcome == hal::CommitOutcome::Deferred) {
            // The scheduler started the engine; retry once it stops again.
            return false;
        }
        recordFailure(nowMs, outcome == hal::CommitOutcome::Locked);
        return false;
    }
    grooves_.markCommitted();
    state_.commitSucceeded();
    patterns_.markCommitted();
    steps_.markCommitted();
    consecutiveFailures_ = 0U;
    retryScheduled_ = false;
    debounceActive_ = false;
    status_ = Status::Ok;
    return storage_.lastCommitOutcome() == hal::CommitOutcome::Committed;
}

bool PersistenceCoordinator::service(const std::uint32_t nowMs, const bool allowFlashWrite) {
    const bool locked = storage_.writeLocked();
    if (locked && status_ != Status::SaveLocked) {
        status_ = Status::SaveLocked;
        ++noticeRevision_;
    } else if (!locked && status_ == Status::SaveLocked) {
        status_ = Status::Ok;
        consecutiveFailures_ = 0U;
        retryScheduled_ = false;
    }

    if (!hasPendingCommit()) {
        debounceActive_ = false;
        if (!locked) {
            status_ = Status::Ok;
            consecutiveFailures_ = 0U;
            retryScheduled_ = false;
        }
        return false;
    }

    const std::uint32_t revision = combinedRevision();
    if (!debounceActive_) {
        debounceActive_ = true;
        firstChangeMs_ = nowMs;
        lastChangeMs_ = nowMs;
        lastRevision_ = revision;
    } else if (revision != lastRevision_) {
        // Every further change restarts the quiet period (F-020).
        lastRevision_ = revision;
        lastChangeMs_ = nowMs;
    }

    const bool due = hasUrgentCommit() ||
        nowMs - lastChangeMs_ >= config::kPersistenceCommitDelayMs ||
        nowMs - firstChangeMs_ >= config::kPersistenceMaximumLatencyMs;
    if (!due || !allowFlashWrite) {
        return false;
    }
    if (retryScheduled_ && static_cast<std::int32_t>(nowMs - retryAtMs_) < 0) {
        return false;
    }
    return commitAll(nowMs);
}

}  // namespace clockfw::services
