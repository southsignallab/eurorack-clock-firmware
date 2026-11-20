/**
 * @file persistent_state_service.h
 * @brief CRC-protected persistence for the current configuration and named user presets.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "domain/clock_types.h"
#include "hal/persistent_layout.h"
#include "hal/persistent_storage.h"

namespace clockfw::services {

/**
 * @brief Owns the durable representation of all user-editable clock configuration.
 *
 * One automatically maintained CURRENT record stores the complete application
 * configuration. Eight additional named user slots can be saved and loaded
 * explicitly. Records are serialized field-by-field so Flash data never depends
 * on C++ structure padding or compiler ABI details.
 *
 * The transport value is persisted as historical user intent, but boot safety is
 * stronger than persistence: ClockApplication always starts the runtime transport
 * in STOP and never auto-starts from a stored PLAY value.
 */
class PersistentStateService final {
  public:
    /** Number of explicit user preset slots. */
    static constexpr std::uint8_t kUserPresetSlotCount = 8U;

    /** Maximum number of user-visible characters in one preset name. */
    static constexpr std::size_t kPresetNameLength = 16U;

    /** Serialized v13 payload bytes for one complete ClockState. */
    static constexpr std::size_t kCurrentStatePayloadBytes = 295U;

    /** Durable CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kCurrentRecordBytes = 307U;

    /** Durable named-preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kPresetRecordBytes = 323U;

    /** Bytes occupied by CURRENT plus all eight V1 named presets. */
    static constexpr std::size_t kSettingsPresetFootprintBytes =
        kCurrentRecordBytes + static_cast<std::size_t>(kUserPresetSlotCount) * kPresetRecordBytes;

    /** Free bytes before the historical score-compatibility region begins. */
    static constexpr std::size_t kBytesBeforeLegacyScoreRegion =
        hal::persistent_layout::kLegacyScoreRegionOffset - kSettingsPresetFootprintBytes;

    /**
     * Maximum payload growth that can be repeated in CURRENT plus eight presets
     * without redesigning the V1 persistence layout.
     *
     * One added serialized state byte consumes nine bytes in the logical image:
     * one in CURRENT and one in each of the eight named preset records.
     */
    static constexpr std::size_t kMaximumInPlaceStatePayloadGrowthBytes =
        kBytesBeforeLegacyScoreRegion / (static_cast<std::size_t>(kUserPresetSlotCount) + 1U);

    /**
     * @brief Constructs the service around the non-volatile storage HAL.
     * @param storage Storage boundary used for validated record reads and writes.
     */
    explicit PersistentStateService(hal::PersistentStorage& storage);

    /** @brief Loads and validates CURRENT plus all preset-slot metadata. */
    void begin();

    /**
     * @brief Restores the complete CURRENT configuration when a valid record exists.
     * @param state Destination state receiving the persisted configuration.
     * @return True when a valid current-state record was restored.
     */
    [[nodiscard]] bool restoreCurrentState(ClockState& state) const;

    /**
     * @brief Schedules a wear-coalesced save of the complete current configuration.
     * @param state Current mutable application configuration.
     * @param nowMs Current monotonic time in milliseconds.
     *
     * The runtime boot-forced STOP value does not overwrite the user's previously
     * persisted transport preference unless requestTransportState() is called.
     */
    void requestCurrentState(const ClockState& state, std::uint32_t nowMs);

    /**
     * @brief Updates the transport value that belongs in the durable CURRENT record.
     * @param transport New transport state explicitly selected by the user.
     * @param nowMs Current monotonic time in milliseconds.
     */
    void requestTransportState(TransportState transport, std::uint32_t nowMs);

    /**
     * @brief Commits pending durable records when Flash writes are timing-safe.
     * @param nowMs Current monotonic time in milliseconds.
     * @param allowFlashCommit False while transport is PLAYING on STM32 targets.
     *
     * STM32F4 stalls Flash reads during erase/program operations. The application
     * therefore defers every physical persistence commit while gates are running.
     * Explicit preset saves are staged in RAM and are flushed as soon as transport
     * is PAUSED or STOPPED.
     */
    void service(std::uint32_t nowMs, bool allowFlashCommit = true);

    /**
     * @brief Reports whether CURRENT, a preset, or a boot migration awaits a durable commit.
     * @return True while any such work is pending.
     */
    bool hasPendingCommit() const;

    /**
     * @brief Reports pending work that is committed without debounce.
     * @return True while a preset edit or a boot migration is pending.
     */
    bool hasUrgentCommit() const;

    /**
     * @brief Revision counter of pending durable content.
     * @return Counter that increments with every accepted change; wraps modulo 2^32.
     */
    std::uint32_t changeRevision() const {
        return changeRevision_;
    }

    /**
     * @brief Stages every pending record into the storage update the caller opened.
     * @param includeCurrent Also stage a pending CURRENT record.
     * @return False when a pending migration or a record could not be staged; pending
     *         flags are kept so the work can be retried.
     */
    [[nodiscard]] bool stagePendingRecords(bool includeCurrent = true);

    /** @brief Marks everything staged by stagePendingRecords() as durable. */
    void commitSucceeded();

    /**
     * @brief Records a failed commit; schedules the standalone migration retry.
     * @param nowMs Current monotonic time in milliseconds; a pending migration is retried
     *              kMigrationRetryIntervalMs later.
     */
    void commitFailed(std::uint32_t nowMs);

    /**
     * @brief Reports whether a valid CURRENT record is known.
     * @return True when a valid CURRENT record was loaded or written.
     */
    bool hasStoredCurrentState() const;

    /**
     * @return Persisted user transport preference, or STOP when CURRENT is invalid.
     * @brief This value is informational and must never be interpreted as an auto-start request.
     */
    TransportState storedTransportState() const;

    /**
     * @brief Compatibility alias retained for tests and prerelease callers.
     * @return Same value as hasStoredCurrentState().
     */
    bool hasStoredTransportState() const;

    /**
     * @brief Saves the complete current configuration into one named user slot.
     * @param slotIndex Zero-based slot index in the range 0..7.
     * @param name User-visible preset name; invalid characters are replaced with spaces.
     * @param state Configuration snapshot to persist.
     * @return True when the record was validated and staged for durable storage.
     */
    [[nodiscard]] bool savePreset(
        std::uint8_t slotIndex,
        const char* name,
        const ClockState& state);

    /**
     * @brief Loads a complete user preset while preserving live transport state.
     * @param slotIndex Zero-based slot index in the range 0..7.
     * @param state Mutable application state receiving the stored configuration.
     * @return True when the requested preset exists and validates successfully.
     */
    [[nodiscard]] bool loadPreset(std::uint8_t slotIndex, ClockState& state) const;

    /**
     * @brief Renames an existing user preset without changing its configuration.
     * @param slotIndex Zero-based slot index in the range 0..7.
     * @param name New user-visible preset name; normalized like savePreset().
     * @return True when the slot existed and the renamed record was staged for durable
     *         storage (the Flash write happens later in service()).
     */
    [[nodiscard]] bool renamePreset(std::uint8_t slotIndex, const char* name);

    /**
     * @brief Erases one logical preset slot by staging an erased (0xFF) record.
     * @param slotIndex Zero-based slot index in the range 0..7.
     * @return True when the slot index was valid and the clear was staged for durable
     *         storage; also true for a slot that was already empty.
     */
    [[nodiscard]] bool clearPreset(std::uint8_t slotIndex);

    /**
     * @brief Reports whether one user slot holds a preset, including staged edits.
     * @param slotIndex Zero-based slot index; out-of-range values report false.
     * @return True when the slot contains (or will contain after commit) a valid preset.
     */
    bool presetExists(std::uint8_t slotIndex) const;

    /**
     * @brief Copies one preset name into a caller-owned null-terminated buffer.
     * @param slotIndex Zero-based preset slot.
     * @param destination Output text buffer.
     * @param destinationSize Output buffer size including terminator.
     */
    void presetName(std::uint8_t slotIndex, char* destination, std::size_t destinationSize) const;

    /**
     * @brief Atomically replaces the complete persistent image with factory state.
     *
     * This clears CURRENT, all named presets, legacy score records, and all Top-100
     * leaderboards in one power-loss-safe A/B commit, then stores the supplied
     * factory state as the new CURRENT record.
     * @param factoryState Validated configuration stored as the new CURRENT record.
     * @return True only after the replacement image has been committed and verified.
     */
    [[nodiscard]] bool factoryReset(const ClockState& factoryState);

  private:
    /** Serialized field count for one complete ClockState payload. */
    static constexpr std::size_t kStatePayloadSize = kCurrentStatePayloadBytes;

    /** CURRENT record size: header + state payload + CRC-32. */
    static constexpr std::size_t kCurrentRecordSize = kCurrentRecordBytes;

    /** Preset record size: header + 16-byte name + state payload + CRC-32. */
    static constexpr std::size_t kPresetRecordSize = kPresetRecordBytes;

    /** Current development record schema version. */
    static constexpr std::uint8_t kSchemaVersion = 13U;

    /** v12 schema (firmware 1.1.0 write format) before SYNC AUTOSTART was appended. */
    static constexpr std::uint8_t kV12SchemaVersion = 12U;
    /** v12 state payload bytes. */
    static constexpr std::size_t kV12StatePayloadSize = 294U;
    /** v12 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kV12CurrentRecordSize = 306U;
    /** v12 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kV12PresetRecordSize = 322U;

    /** v11 schema used before Custom Groove slot references were appended. */
    static constexpr std::uint8_t kV11SchemaVersion = 11U;
    /** v11 state payload bytes. */
    static constexpr std::size_t kV11StatePayloadSize = 285U;
    /** v11 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kV11CurrentRecordSize = 297U;
    /** v11 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kV11PresetRecordSize = 313U;

    /** v10 schema used by 1.1 development before configurable input roles. */
    static constexpr std::uint8_t kV10SchemaVersion = 10U;
    /** v10 state payload bytes. */
    static constexpr std::size_t kV10StatePayloadSize = 283U;
    /** v10 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kV10CurrentRecordSize = 295U;
    /** v10 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kV10PresetRecordSize = 311U;

    /** v9 schema used by early 1.1 development before Stage-1 Groove persistence. */
    static constexpr std::uint8_t kV9SchemaVersion = 9U;
    /** v9 state payload bytes. */
    static constexpr std::size_t kV9StatePayloadSize = 256U;
    /** v9 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kV9CurrentRecordSize = 268U;
    /** v9 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kV9PresetRecordSize = 284U;

    /** v8 schema used by 1.0.x before persistent Pre-Count was added. */
    static constexpr std::uint8_t kV8SchemaVersion = 8U;
    /** v8 state payload bytes. */
    static constexpr std::size_t kV8StatePayloadSize = 255U;
    /** v8 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kV8CurrentRecordSize = 267U;
    /** v8 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kV8PresetRecordSize = 283U;

    /** v7 schema used through the 1.0.0 release-prep before SYNC smoothing became persistent. */
    static constexpr std::uint8_t kV7SchemaVersion = 7U;
    /** v7 state payload bytes. */
    static constexpr std::size_t kV7StatePayloadSize = 254U;
    /** v7 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kV7CurrentRecordSize = 266U;
    /** v7 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kV7PresetRecordSize = 282U;

    /** v6 schema used through 0.19.0-beta.7 before device orientation preferences. */
    static constexpr std::uint8_t kV6SchemaVersion = 6U;
    /** v6 state payload bytes. */
    static constexpr std::size_t kV6StatePayloadSize = 252U;
    /** v6 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kV6CurrentRecordSize = 264U;
    /** v6 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kV6PresetRecordSize = 280U;

    /** v5 schema used by alpha.57 before configurable external reset semantics. */
    static constexpr std::uint8_t kV5SchemaVersion = 5U;
    /** v5 state payload bytes. */
    static constexpr std::size_t kV5StatePayloadSize = 251U;
    /** v5 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kV5CurrentRecordSize = 263U;
    /** v5 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kV5PresetRecordSize = 279U;

    /** Earlier v4 schema, before tempo limits and ONE CLOCK humanize were persisted. */
    static constexpr std::uint8_t kPreviousSchemaVersion = 4U;

    /** v4 state payload bytes. */
    static constexpr std::size_t kPreviousStatePayloadSize = 245U;
    /** v4 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kPreviousCurrentRecordSize = 257U;
    /** v4 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kPreviousPresetRecordSize = 273U;

    /** Older v3 schema, before display-protection preferences were persisted. */
    static constexpr std::uint8_t kLegacySchemaVersion = 3U;

    /** v3 state payload bytes. */
    static constexpr std::size_t kLegacyStatePayloadSize = 241U;
    /** v3 CURRENT record bytes: header + state payload + CRC-32. */
    static constexpr std::size_t kLegacyCurrentRecordSize = 253U;
    /** v3 preset record bytes: header + name + state payload + CRC-32. */
    static constexpr std::size_t kLegacyPresetRecordSize = 269U;

    /** First byte offset of the automatically maintained CURRENT record. */
    static constexpr std::size_t kCurrentRecordOffset = 0U;

    /** First byte offset of the explicit user-preset area. */
    static constexpr std::size_t kPresetAreaOffset = kCurrentRecordSize;

    /**
     * @brief Returns the byte offset for one preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t presetOffset(const std::uint8_t slotIndex) {
        return kPresetAreaOffset + static_cast<std::size_t>(slotIndex) * kPresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v12 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t v12PresetOffset(const std::uint8_t slotIndex) {
        return kV12CurrentRecordSize + static_cast<std::size_t>(slotIndex) * kV12PresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v11 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t v11PresetOffset(const std::uint8_t slotIndex) {
        return kV11CurrentRecordSize + static_cast<std::size_t>(slotIndex) * kV11PresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v10 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t v10PresetOffset(const std::uint8_t slotIndex) {
        return kV10CurrentRecordSize + static_cast<std::size_t>(slotIndex) * kV10PresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v9 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t v9PresetOffset(const std::uint8_t slotIndex) {
        return kV9CurrentRecordSize + static_cast<std::size_t>(slotIndex) * kV9PresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v8 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t v8PresetOffset(const std::uint8_t slotIndex) {
        return kV8CurrentRecordSize + static_cast<std::size_t>(slotIndex) * kV8PresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v7 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t v7PresetOffset(const std::uint8_t slotIndex) {
        return kV7CurrentRecordSize + static_cast<std::size_t>(slotIndex) * kV7PresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v6 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t v6PresetOffset(const std::uint8_t slotIndex) {
        return kV6CurrentRecordSize + static_cast<std::size_t>(slotIndex) * kV6PresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v5 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t v5PresetOffset(const std::uint8_t slotIndex) {
        return kV5CurrentRecordSize + static_cast<std::size_t>(slotIndex) * kV5PresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v4 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t previousPresetOffset(const std::uint8_t slotIndex) {
        return kPreviousCurrentRecordSize +
            static_cast<std::size_t>(slotIndex) * kPreviousPresetRecordSize;
    }

    /**
     * @brief Returns the byte offset used by one v3 preset slot.
     * @param slotIndex Zero-based preset slot; the caller keeps it below
     *                  kUserPresetSlotCount (not checked here).
     * @return Byte offset of the slot's record from the start of the records area.
     */
    static constexpr std::size_t legacyPresetOffset(const std::uint8_t slotIndex) {
        return kLegacyCurrentRecordSize +
            static_cast<std::size_t>(slotIndex) * kLegacyPresetRecordSize;
    }

    /**
     * @brief Serializes ClockState into a stable field-by-field byte payload.
     * @param state Configuration to serialize.
     * @return Current-schema (v13) payload, little-endian multi-byte fields.
     */
    static std::array<std::uint8_t, kStatePayloadSize> serializeState(const ClockState& state);

    /**
     * @brief Validates and deserializes one state payload.
     * @param payload Serialized current-schema (v13) state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeState(
        const std::array<std::uint8_t, kStatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Serializes the CURRENT record including metadata and CRC.
     * @param state Configuration stored in the record payload.
     * @return Complete CURRENT record: magic, schema, kind, payload size, payload, CRC-32.
     */
    static std::array<std::uint8_t, kCurrentRecordSize> serializeCurrentRecord(
        const ClockState& state);

    /**
     * @brief Validates and extracts one CURRENT record.
     * @param record Raw current-schema (v13) CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeCurrentRecord(
        const std::array<std::uint8_t, kCurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Serializes one named user preset record including metadata and CRC.
     * @param name User-visible name; normalized with normalizePresetName().
     * @param state Configuration stored in the record payload.
     * @return Complete preset record: header, 16-character name, payload, CRC-32.
     */
    static std::array<std::uint8_t, kPresetRecordSize> serializePresetRecord(
        const char* name,
        const ClockState& state);

    /**
     * @brief Validates and extracts one named user preset record.
     * @param record Raw current-schema (v13) preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializePresetRecord(
        const std::array<std::uint8_t, kPresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v12 payload by injecting the SYNC AUTOSTART default (OFF).
     * @param payload Serialized v12 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeV12State(
        const std::array<std::uint8_t, kV12StatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v12 CURRENT record.
     * @param record Raw v12 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV12CurrentRecord(
        const std::array<std::uint8_t, kV12CurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v12 preset record.
     * @param record Raw v12 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV12PresetRecord(
        const std::array<std::uint8_t, kV12PresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v11 payload by injecting Custom Groove slot defaults.
     * @param payload Serialized v11 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeV11State(
        const std::array<std::uint8_t, kV11StatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v11 CURRENT record.
     * @param record Raw v11 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV11CurrentRecord(
        const std::array<std::uint8_t, kV11CurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v11 preset record.
     * @param record Raw v11 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV11PresetRecord(
        const std::array<std::uint8_t, kV11PresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v10 payload by injecting factory input-role assignments.
     * @param payload Serialized v10 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeV10State(
        const std::array<std::uint8_t, kV10StatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v10 CURRENT record.
     * @param record Raw v10 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV10CurrentRecord(
        const std::array<std::uint8_t, kV10CurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v10 preset record.
     * @param record Raw v10 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV10PresetRecord(
        const std::array<std::uint8_t, kV10PresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v9 payload by injecting Stage-1 Groove defaults.
     * @param payload Serialized v9 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeV9State(
        const std::array<std::uint8_t, kV9StatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v9 CURRENT record.
     * @param record Raw v9 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV9CurrentRecord(
        const std::array<std::uint8_t, kV9CurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v9 preset record.
     * @param record Raw v9 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV9PresetRecord(
        const std::array<std::uint8_t, kV9PresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v8 state payload by injecting the factory Pre-Count length.
     * @param payload Serialized v8 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeV8State(
        const std::array<std::uint8_t, kV8StatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v8 CURRENT record.
     * @param record Raw v8 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV8CurrentRecord(
        const std::array<std::uint8_t, kV8CurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v8 preset record.
     * @param record Raw v8 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV8PresetRecord(
        const std::array<std::uint8_t, kV8PresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v7 state payload by injecting the factory SYNC smoothing mode.
     * @param payload Serialized v7 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeV7State(
        const std::array<std::uint8_t, kV7StatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v7 CURRENT record.
     * @param record Raw v7 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV7CurrentRecord(
        const std::array<std::uint8_t, kV7CurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v7 preset record.
     * @param record Raw v7 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV7PresetRecord(
        const std::array<std::uint8_t, kV7PresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v6 state payload by injecting device-orientation defaults.
     * @param payload Serialized v6 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeV6State(
        const std::array<std::uint8_t, kV6StatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v6 CURRENT record.
     * @param record Raw v6 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV6CurrentRecord(
        const std::array<std::uint8_t, kV6CurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v6 preset record.
     * @param record Raw v6 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV6PresetRecord(
        const std::array<std::uint8_t, kV6PresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v5 state payload by injecting the factory RST interpretation.
     * @param payload Serialized v5 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeV5State(
        const std::array<std::uint8_t, kV5StatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v5 CURRENT record.
     * @param record Raw v5 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV5CurrentRecord(
        const std::array<std::uint8_t, kV5CurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v5 preset record.
     * @param record Raw v5 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeV5PresetRecord(
        const std::array<std::uint8_t, kV5PresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v4 CURRENT record to the current in-memory state.
     * @param record Raw v4 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializePreviousCurrentRecord(
        const std::array<std::uint8_t, kPreviousCurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v4 preset record.
     * @param record Raw v4 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializePreviousPresetRecord(
        const std::array<std::uint8_t, kPreviousPresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v4 state payload by injecting tempo-range and humanize defaults.
     * @param payload Serialized v4 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializePreviousState(
        const std::array<std::uint8_t, kPreviousStatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Validates and upgrades one v3 CURRENT record to the current in-memory state.
     * @param record Raw v3 CURRENT record as read from storage.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeLegacyCurrentRecord(
        const std::array<std::uint8_t, kLegacyCurrentRecordSize>& record,
        ClockState& state);

    /**
     * @brief Validates and upgrades one named v3 preset record.
     * @param record Raw v3 preset record as read from storage.
     * @param name Destination for the stored name (kPresetNameLength + 1 bytes,
     *             null-terminated); unspecified when false is returned.
     * @param state Destination state; written only when true is returned.
     * @return True when header, payload size, CRC-32 and the decoded state validate.
     */
    static bool deserializeLegacyPresetRecord(
        const std::array<std::uint8_t, kLegacyPresetRecordSize>& record,
        char* name,
        ClockState& state);

    /**
     * @brief Upgrades a v3 state payload by injecting current display-protection defaults.
     * @param payload Serialized v3 state payload.
     * @param state Destination state; written only when true is returned.
     * @return True when the decoded (upgraded) state validates.
     */
    static bool deserializeLegacyState(
        const std::array<std::uint8_t, kLegacyStatePayloadSize>& payload,
        ClockState& state);

    /**
     * @brief Calculates standard reflected CRC-32 over a bounded byte sequence.
     * @param data First byte of the sequence.
     * @param size Number of bytes.
     * @return CRC-32 as computed by hal::crc32().
     */
    static std::uint32_t calculateCrc32(const std::uint8_t* data, std::size_t size);

    /**
     * @brief Checks that all serialized enum/range values form a valid ClockState.
     * @param state Configuration to check.
     * @return Result of isPersistentStateValid().
     */
    static bool isStateValid(const ClockState& state);

    /**
     * @brief Converts a user name into the fixed 16-character storage representation.
     *
     * Lowercase letters become uppercase, unsupported characters and the tail after
     * the terminator become spaces.
     * @param source Null-terminated user name; nullptr yields an all-space name.
     * @param destination Output buffer of kPresetNameLength + 1 bytes, null-terminated.
     */
    static void normalizePresetName(const char* source, char* destination);

    /**
     * @brief Compares two complete states without relying on structure padding.
     * @param first First state.
     * @param second Second state.
     * @return True when both serialize to identical payloads.
     */
    static bool statesEqual(const ClockState& first, const ClockState& second);

    /**
     * @brief Reads one preset slot in the current schema or, failing that, any supported
     *        older layout.
     * @param slotIndex Preset slot in [0, kUserPresetSlotCount).
     * @param name Destination for the normalized preset name (kPresetNameLength + 1 bytes).
     * @param state Destination state.
     * @param priorSchema Set true when the record was found in an older layout.
     * @return True when a valid record was found.
     */
    [[nodiscard]] bool loadPresetAnySchema(
        std::uint8_t slotIndex,
        char* name,
        ClockState& state,
        bool& priorSchema) const;

    /**
     * @brief Stages the canonical current-schema records area from the stored (possibly
     *        older) layout.
     *
     * Requires an open storage update. CURRENT is staged from storedCurrentState_;
     * every valid preset is re-read from Flash and re-serialized at its current offset.
     * @return False when a fill, a preset read or a stage operation failed.
     */
    [[nodiscard]] bool stageMigratedRecords();

    /** @brief Delay between retries of a failed boot migration commit. */
    static constexpr std::uint32_t kMigrationRetryIntervalMs = 3000U;

    /**
     * @brief Reports staged preset edits.
     * @return True while at least one preset slot has a staged edit.
     */
    bool presetWritePending() const;

    /** Revision counter returned by changeRevision(). */
    std::uint32_t changeRevision_ = 0U;
    /** True when the last stagePendingRecords() included the CURRENT record. */
    bool currentStaged_ = false;

    /** Storage boundary; not owned. */
    hal::PersistentStorage& storage_;
    /** Last CURRENT state known to be durable. */
    ClockState storedCurrentState_{};
    /** CURRENT state awaiting commit (transport replaced by the persisted preference). */
    ClockState pendingCurrentState_{};
    /** Durable preset names, null-terminated; empty for invalid slots. */
    std::array<std::array<char, kPresetNameLength + 1U>, kUserPresetSlotCount> presetNames_{};
    /** True for each slot that holds a durable valid preset. */
    std::array<bool, kUserPresetSlotCount> presetValid_{};
    /** Transport value written into CURRENT; never used as an auto-start request. */
    TransportState persistedTransportPreference_ = TransportState::Stopped;
    /** Time in ms when the pending CURRENT change started its commit delay. */
    std::uint32_t pendingSinceMs_ = 0U;
    /** True when storedCurrentState_ holds a valid durable record. */
    bool hasStoredCurrentState_ = false;
    /** True while pendingCurrentState_ differs from the durable CURRENT record. */
    bool writePending_ = false;
    // A boot migration whose commit failed leaves the older layout in Flash. Until
    // it is committed, no other write of this service may touch the records area,
    // because current-schema offsets would overwrite older-layout presets.
    /** True while a boot migration awaits its commit. */
    bool migrationPending_ = false;
    /** True after a failed migration commit until a commit succeeds. */
    bool migrationRetryScheduled_ = false;
    /** Earliest migration retry time in milliseconds. */
    std::uint32_t migrationRetryAtMs_ = 0U;
    // Each accepted operation owns its own slot until an atomic A/B commit.
    // Updating one pending slot may replace its own value, never another slot.
    /** Serialized record staged per slot; all 0xFF for a staged clear. */
    std::array<std::array<std::uint8_t, kPresetRecordSize>, kUserPresetSlotCount>
        pendingPresetRecords_{};
    /** Normalized name staged per slot. */
    std::array<std::array<char, kPresetNameLength + 1U>, kUserPresetSlotCount>
        pendingPresetNames_{};
    /** True for each slot with a staged edit. */
    std::array<bool, kUserPresetSlotCount> pendingPresetWrites_{};
    /** True when the staged edit leaves a valid preset (save), false for a clear. */
    std::array<bool, kUserPresetSlotCount> pendingPresetWillExist_{};
};

static_assert(
    PersistentStateService::kSettingsPresetFootprintBytes <=
        hal::persistent_layout::kLegacyScoreRegionOffset,
    "CURRENT plus named presets overlap the legacy score region.");
static_assert(
    PersistentStateService::kMaximumInPlaceStatePayloadGrowthBytes == 20U,
    "V1 persistence headroom changed; update the forward-compatibility analysis.");

}  // namespace clockfw::services
