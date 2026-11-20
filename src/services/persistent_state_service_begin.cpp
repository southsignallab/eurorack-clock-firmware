/**
 * @file persistent_state_service_begin.cpp
 * @brief Startup validation and migration orchestration for persistent CLOCK state.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "services/persistent_state_service.h"

#include <algorithm>

namespace clockfw::services {
namespace {

/**
 * @brief Reads one CURRENT record shape and decodes it into the current in-memory model.
 * @param storage Persistent byte store containing the record.
 * @param offset First byte of the record in the logical image.
 * @param decoder Schema-specific decoder and migration function.
 * @param state Destination state receiving the decoded record.
 * @return True only when both the bounded read and schema validation succeed.
 *
 * Keeping the record-size-dependent buffer here makes the migration order in begin()
 * readable without hiding any schema-specific validation inside PersistentStorage.
 */
template <std::size_t RecordSize>
bool loadCurrentRecord(
    hal::PersistentStorage& storage,
    const std::size_t offset,
    bool (*decoder)(const std::array<std::uint8_t, RecordSize>&, ClockState&),
    ClockState& state) {
    std::array<std::uint8_t, RecordSize> record{};
    return storage.readBytes(offset, record.data(), record.size()) && decoder(record, state);
}

/**
 * @brief Reads one named preset record shape and decodes it into the current model.
 * @param storage Persistent byte store containing the record.
 * @param offset First byte of the record in the logical image.
 * @param decoder Schema-specific preset decoder and migration function.
 * @param name Destination buffer for the normalized preset name.
 * @param state Destination state receiving the decoded record.
 * @return True only when both the bounded read and schema validation succeed.
 */
template <std::size_t RecordSize>
bool loadPresetRecord(
    hal::PersistentStorage& storage,
    const std::size_t offset,
    bool (*decoder)(const std::array<std::uint8_t, RecordSize>&, char*, ClockState&),
    char* name,
    ClockState& state) {
    std::array<std::uint8_t, RecordSize> record{};
    return storage.readBytes(offset, record.data(), record.size()) && decoder(record, name, state);
}

}  // namespace

void PersistentStateService::begin() {
    pendingPresetWrites_.fill(false);
    pendingPresetWillExist_.fill(false);
    for (auto& name : pendingPresetNames_) {
        name.fill('\0');
    }

    ClockState loadedCurrent{};
    hasStoredCurrentState_ =
        loadCurrentRecord(storage_, kCurrentRecordOffset, deserializeCurrentRecord, loadedCurrent);

    // Migration always probes newest-to-oldest. The first valid record wins, so a
    // newer schema can never be shadowed by stale bytes that happen to resemble an
    // older record layout.
    bool migrationNeeded = false;
    if (!hasStoredCurrentState_) {
        migrationNeeded =
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializeV12CurrentRecord, loadedCurrent) ||
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializeV11CurrentRecord, loadedCurrent) ||
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializeV10CurrentRecord, loadedCurrent) ||
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializeV9CurrentRecord, loadedCurrent) ||
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializeV8CurrentRecord, loadedCurrent) ||
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializeV7CurrentRecord, loadedCurrent) ||
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializeV6CurrentRecord, loadedCurrent) ||
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializeV5CurrentRecord, loadedCurrent) ||
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializePreviousCurrentRecord, loadedCurrent) ||
            loadCurrentRecord(
                storage_, kCurrentRecordOffset, deserializeLegacyCurrentRecord, loadedCurrent);
        hasStoredCurrentState_ = migrationNeeded;
    }

    if (hasStoredCurrentState_) {
        storedCurrentState_ = loadedCurrent;
        pendingCurrentState_ = loadedCurrent;
        persistedTransportPreference_ = loadedCurrent.transport;
    } else {
        // GCC 12.3 for ARM can ICE while lowering assignment from an anonymous
        // aggregate temporary (ClockState{}). A named default-initialized object
        // produces identical C++ semantics without exercising that compiler path.
        const ClockState defaultState{};
        storedCurrentState_ = defaultState;
        pendingCurrentState_ = defaultState;
        persistedTransportPreference_ = TransportState::Stopped;
    }
    writePending_ = false;

    for (std::uint8_t slotIndex = 0U; slotIndex < kUserPresetSlotCount; ++slotIndex) {
        ClockState loadedPreset{};
        char name[kPresetNameLength + 1U]{};
        bool loadedPriorSchema = false;
        presetValid_[slotIndex] =
            loadPresetAnySchema(slotIndex, name, loadedPreset, loadedPriorSchema);
        migrationNeeded = migrationNeeded || loadedPriorSchema;
        presetNames_[slotIndex].fill('\0');
        if (presetValid_[slotIndex]) {
            std::copy_n(name, kPresetNameLength, presetNames_[slotIndex].begin());
            presetNames_[slotIndex][kPresetNameLength] = '\0';
        }
    }

    // Rewrite the records area in the current schema in one A/B transaction. A
    // failed commit is not ignored: the older layout stays authoritative and the
    // migration is retried by service() before any other write of this service.
    migrationPending_ = false;
    migrationRetryScheduled_ = false;
    if (migrationNeeded) {
        const bool committed =
            storage_.beginUpdate() && stageMigratedRecords() && storage_.commitUpdate();
        if (!committed) {
            storage_.cancelUpdate();
            migrationPending_ = true;
        }
    }
}

bool PersistentStateService::loadPresetAnySchema(
    const std::uint8_t slotIndex,
    char* const name,
    ClockState& state,
    bool& priorSchema) const {
    priorSchema = false;
    if (loadPresetRecord(storage_, presetOffset(slotIndex), deserializePresetRecord, name, state)) {
        return true;
    }
    priorSchema =
        loadPresetRecord(
            storage_, v12PresetOffset(slotIndex), deserializeV12PresetRecord, name, state) ||
        loadPresetRecord(
            storage_, v11PresetOffset(slotIndex), deserializeV11PresetRecord, name, state) ||
        loadPresetRecord(
            storage_, v10PresetOffset(slotIndex), deserializeV10PresetRecord, name, state) ||
        loadPresetRecord(
            storage_, v9PresetOffset(slotIndex), deserializeV9PresetRecord, name, state) ||
        loadPresetRecord(
            storage_, v8PresetOffset(slotIndex), deserializeV8PresetRecord, name, state) ||
        loadPresetRecord(
            storage_, v7PresetOffset(slotIndex), deserializeV7PresetRecord, name, state) ||
        loadPresetRecord(
            storage_, v6PresetOffset(slotIndex), deserializeV6PresetRecord, name, state) ||
        loadPresetRecord(
            storage_, v5PresetOffset(slotIndex), deserializeV5PresetRecord, name, state) ||
        loadPresetRecord(
            storage_,
            previousPresetOffset(slotIndex),
            deserializePreviousPresetRecord,
            name,
            state) ||
        loadPresetRecord(
            storage_, legacyPresetOffset(slotIndex), deserializeLegacyPresetRecord, name, state);
    return priorSchema;
}

bool PersistentStateService::stageMigratedRecords() {
    // The bounded staging image holds the migrated records; this avoids keeping
    // eight migrated ClockState objects or a complete image on the embedded stack.
    constexpr std::size_t kRecordsEnd =
        kCurrentRecordSize + kUserPresetSlotCount * kPresetRecordSize;
    if (!storage_.stageFill(0U, kRecordsEnd, static_cast<std::uint8_t>(0xFFU))) {
        return false;
    }
    if (hasStoredCurrentState_) {
        const auto migratedRecord = serializeCurrentRecord(storedCurrentState_);
        if (!storage_.stageBytes(
                kCurrentRecordOffset, migratedRecord.data(), migratedRecord.size())) {
            return false;
        }
    }
    for (std::uint8_t slotIndex = 0U; slotIndex < kUserPresetSlotCount; ++slotIndex) {
        if (!presetValid_[slotIndex]) {
            continue;
        }
        ClockState loadedPreset{};
        char name[kPresetNameLength + 1U]{};
        bool ignoredPriorSchema = false;
        if (!loadPresetAnySchema(slotIndex, name, loadedPreset, ignoredPriorSchema)) {
            return false;
        }
        const auto migratedRecord = serializePresetRecord(name, loadedPreset);
        if (!storage_.stageBytes(
                presetOffset(slotIndex), migratedRecord.data(), migratedRecord.size())) {
            return false;
        }
    }
    return true;
}

}  // namespace clockfw::services
