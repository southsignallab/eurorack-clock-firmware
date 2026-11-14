/**
 * @file persistent_state_service.cpp
 * @brief Durable CURRENT-state and named-preset storage orchestration.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "services/persistent_state_service.h"

#include <algorithm>
#include <cstring>
#include "config.h"
namespace clockfw::services {

PersistentStateService::PersistentStateService(hal::PersistentStorage& storage) : storage_(storage) {}

bool PersistentStateService::restoreCurrentState(ClockState& state) const {
    if (!hasStoredCurrentState_) {
        return false;
    }
    state = storedCurrentState_;
    return true;
}

void PersistentStateService::requestCurrentState(
    const ClockState& state,
    const std::uint32_t nowMs) {
    ClockState durableState = state;
    durableState.transport = persistedTransportPreference_;

    if (hasStoredCurrentState_ && statesEqual(durableState, storedCurrentState_)) {
        pendingCurrentState_ = durableState;
        writePending_ = false;
        return;
    }

    if (!writePending_ || !statesEqual(durableState, pendingCurrentState_)) {
        pendingCurrentState_ = durableState;
        pendingSinceMs_ = nowMs;
        writePending_ = true;
        ++changeRevision_;
    }
}

void PersistentStateService::requestTransportState(
    const TransportState transport,
    const std::uint32_t nowMs) {
    const bool preferenceChanged = transport != persistedTransportPreference_;
    persistedTransportPreference_ = transport;
    pendingCurrentState_.transport = transport;

    if (hasStoredCurrentState_ && transport == storedCurrentState_.transport &&
        statesEqual(pendingCurrentState_, storedCurrentState_)) {
        writePending_ = false;
        return;
    }

    // Repeating an identical pending transport command must not postpone the
    // wear-coalescing deadline indefinitely. Restart only when the value changed.
    if (!writePending_ || preferenceChanged) {
        pendingSinceMs_ = nowMs;
        ++changeRevision_;
    }
    writePending_ = true;
}

bool PersistentStateService::presetWritePending() const {
    return std::any_of(pendingPresetWrites_.begin(), pendingPresetWrites_.end(), [](const bool pending) {
        return pending;
    });
}

bool PersistentStateService::hasUrgentCommit() const {
    return migrationPending_ || presetWritePending();
}

bool PersistentStateService::hasPendingCommit() const {
    return writePending_ || hasUrgentCommit();
}

bool PersistentStateService::stagePendingRecords(const bool includeCurrent) {
    // A pending boot migration always rides in the same transaction as, and
    // underneath, every other accepted mutation.
    bool staged = !migrationPending_ || stageMigratedRecords();
    currentStaged_ = includeCurrent && writePending_;
    if (staged && currentStaged_) {
        const std::array<std::uint8_t, kCurrentRecordSize> currentRecord =
            serializeCurrentRecord(pendingCurrentState_);
        staged = storage_.stageBytes(kCurrentRecordOffset, currentRecord.data(), currentRecord.size());
    }
    // Pending flags are cleared only by commitSucceeded(); a failed stage or
    // commit leaves every slot retryable.
    for (std::uint8_t slot = 0U; staged && slot < kUserPresetSlotCount; ++slot) {
        if (pendingPresetWrites_[slot]) {
            staged = storage_.stageBytes(
                presetOffset(slot), pendingPresetRecords_[slot].data(), pendingPresetRecords_[slot].size());
        }
    }
    return staged;
}

void PersistentStateService::commitFailed(const std::uint32_t nowMs) {
    if (migrationPending_) {
        migrationRetryScheduled_ = true;
        migrationRetryAtMs_ = nowMs + kMigrationRetryIntervalMs;
    }
}

void PersistentStateService::commitSucceeded() {
    migrationPending_ = false;
    migrationRetryScheduled_ = false;
    if (currentStaged_) {
        storedCurrentState_ = pendingCurrentState_;
        persistedTransportPreference_ = storedCurrentState_.transport;
        hasStoredCurrentState_ = true;
        writePending_ = false;
        currentStaged_ = false;
    }
    for (std::uint8_t slot = 0U; slot < kUserPresetSlotCount; ++slot) {
        if (!pendingPresetWrites_[slot]) {
            continue;
        }
        presetValid_[slot] = pendingPresetWillExist_[slot];
        presetNames_[slot].fill('\0');
        if (pendingPresetWillExist_[slot]) {
            std::copy_n(pendingPresetNames_[slot].begin(), kPresetNameLength, presetNames_[slot].begin());
        }
        pendingPresetWrites_[slot] = false;
    }
}

void PersistentStateService::service(
    const std::uint32_t nowMs,
    const bool allowFlashCommit) {
    if (!allowFlashCommit) {
        return;
    }
    const bool migrate = migrationPending_;
    if (migrate && migrationRetryScheduled_ &&
        static_cast<std::int32_t>(nowMs - migrationRetryAtMs_) < 0) {
        return;
    }
    const bool hasPendingPresets = presetWritePending();
    // An explicit user save is already a synchronization point. Include a
    // pending CURRENT record in the same whole-sector Flash commit rather than
    // causing a second erase/program cycle a few seconds later.
    const bool currentRecordReady = writePending_ &&
        (hasPendingPresets || nowMs - pendingSinceMs_ >= config::kPersistenceCommitDelayMs);
    if (!migrate && !currentRecordReady && !hasPendingPresets) {
        return;
    }
    if (!storage_.beginUpdate() || !stagePendingRecords(currentRecordReady) || !storage_.commitUpdate()) {
        storage_.cancelUpdate();
        commitFailed(nowMs);
        return;
    }
    commitSucceeded();
}

bool PersistentStateService::hasStoredCurrentState() const {
    return hasStoredCurrentState_;
}

TransportState PersistentStateService::storedTransportState() const {
    return hasStoredCurrentState_ ? storedCurrentState_.transport : TransportState::Stopped;
}

bool PersistentStateService::hasStoredTransportState() const {
    return hasStoredCurrentState();
}

bool PersistentStateService::savePreset(
    const std::uint8_t slotIndex,
    const char* const name,
    const ClockState& state) {
    if (slotIndex >= kUserPresetSlotCount || name == nullptr || !isStateValid(state)) {
        return false;
    }

    pendingPresetRecords_[slotIndex] = serializePresetRecord(name, state);
    pendingPresetWillExist_[slotIndex] = true;
    pendingPresetWrites_[slotIndex] = true;
    ++changeRevision_;

    char normalizedName[kPresetNameLength + 1U]{};
    normalizePresetName(name, normalizedName);
    pendingPresetNames_[slotIndex].fill('\0');
    std::copy_n(normalizedName, kPresetNameLength, pendingPresetNames_[slotIndex].begin());
    return true;
}

bool PersistentStateService::loadPreset(
    const std::uint8_t slotIndex,
    ClockState& state) const {
    if (slotIndex >= kUserPresetSlotCount || !presetExists(slotIndex)) {
        return false;
    }

    ClockState loaded{};
    char ignoredName[kPresetNameLength + 1U]{};
    if (pendingPresetWrites_[slotIndex]) {
        if (!deserializePresetRecord(pendingPresetRecords_[slotIndex], ignoredName, loaded)) {
            return false;
        }
    } else {
        // While a boot migration is pending, Flash may still hold an older layout.
        bool ignoredPriorSchema = false;
        if (!loadPresetAnySchema(slotIndex, ignoredName, loaded, ignoredPriorSchema)) {
            return false;
        }
    }

    // Loading a musical preset must never cause an implicit transport transition
    // or change device-local panel/display orientation preferences.
    const TransportState liveTransport = state.transport;
    const DevicePreferences liveDevicePreferences = state.device;
    state = loaded;
    state.transport = liveTransport;
    state.device = liveDevicePreferences;
    return true;
}

bool PersistentStateService::renamePreset(
    const std::uint8_t slotIndex,
    const char* const name) {
    if (slotIndex >= kUserPresetSlotCount || !presetExists(slotIndex) || name == nullptr) {
        return false;
    }

    ClockState state{};
    char ignoredName[kPresetNameLength + 1U]{};
    if (pendingPresetWrites_[slotIndex]) {
        if (!deserializePresetRecord(pendingPresetRecords_[slotIndex], ignoredName, state)) {
            return false;
        }
    } else {
        bool ignoredPriorSchema = false;
        if (!loadPresetAnySchema(slotIndex, ignoredName, state, ignoredPriorSchema)) {
            return false;
        }
    }
    return savePreset(slotIndex, name, state);
}

bool PersistentStateService::clearPreset(const std::uint8_t slotIndex) {
    if (slotIndex >= kUserPresetSlotCount) {
        return false;
    }

    pendingPresetRecords_[slotIndex].fill(0xFFU);
    pendingPresetNames_[slotIndex].fill('\0');
    pendingPresetWillExist_[slotIndex] = false;
    pendingPresetWrites_[slotIndex] = true;
    ++changeRevision_;
    return true;
}

bool PersistentStateService::presetExists(const std::uint8_t slotIndex) const {
    if (slotIndex >= kUserPresetSlotCount) {
        return false;
    }
    if (pendingPresetWrites_[slotIndex]) {
        return pendingPresetWillExist_[slotIndex];
    }
    return presetValid_[slotIndex];
}

void PersistentStateService::presetName(
    const std::uint8_t slotIndex,
    char* const destination,
    const std::size_t destinationSize) const {
    if (destination == nullptr || destinationSize == 0U) {
        return;
    }
    destination[0] = '\0';
    if (!presetExists(slotIndex)) {
        return;
    }

    if (pendingPresetWrites_[slotIndex]) {
        const std::size_t copyLength = std::min(kPresetNameLength, destinationSize - 1U);
        std::copy_n(pendingPresetNames_[slotIndex].data(), copyLength, destination);
        destination[copyLength] = '\0';
        std::size_t end = std::strlen(destination);
        while (end > 0U && destination[end - 1U] == ' ') {
            destination[--end] = '\0';
        }
        return;
    }

    const std::size_t copyLength = std::min(kPresetNameLength, destinationSize - 1U);
    std::copy_n(presetNames_[slotIndex].data(), copyLength, destination);
    destination[copyLength] = '\0';

    // Remove right-padding spaces for a compact OLED list representation.
    std::size_t end = std::strlen(destination);
    while (end > 0U && destination[end - 1U] == ' ') {
        destination[--end] = '\0';
    }
}
}  // namespace clockfw::services
