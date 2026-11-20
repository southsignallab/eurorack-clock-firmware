/**
 * @file persistent_state_migration_v12.cpp
 * @brief Backward-compatible v12 (firmware 1.1.0) to v13 migration for SYNC AUTOSTART.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "services/persistent_state_service.h"

#include <algorithm>

namespace clockfw::services {
namespace {

/** @brief Reads one little-endian 16-bit value from a legacy v12 record. */
std::uint16_t readUint16LeV12(const std::uint8_t* const source) {
    return static_cast<std::uint16_t>(source[0]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(source[1]) << 8U);
}

/** @brief Reads one little-endian 32-bit value from a legacy v12 record. */
std::uint32_t readUint32LeV12(const std::uint8_t* const source) {
    return static_cast<std::uint32_t>(source[0]) | (static_cast<std::uint32_t>(source[1]) << 8U) |
        (static_cast<std::uint32_t>(source[2]) << 16U) |
        (static_cast<std::uint32_t>(source[3]) << 24U);
}

/** @brief Returns whether a v12 preset-name byte belongs to the supported alphabet. */
bool isAllowedV12PresetCharacter(const char character) {
    return character == ' ' || character == '-' || character == '_' ||
        (character >= '0' && character <= '9') || (character >= 'A' && character <= 'Z');
}
}  // namespace

bool PersistentStateService::deserializeV12State(
    const std::array<std::uint8_t, kV12StatePayloadSize>& payload,
    ClockState& state) {
    static_assert(kStatePayloadSize == kV12StatePayloadSize + 1U);
    std::array<std::uint8_t, kStatePayloadSize> upgraded{};
    std::copy(payload.begin(), payload.end(), upgraded.begin());

    // Schema v13 appends one SYNC AUTOSTART byte. Zero-initialization selects OFF,
    // the factory setting (decision D-01).
    return deserializeState(upgraded, state);
}

bool PersistentStateService::deserializeV12CurrentRecord(
    const std::array<std::uint8_t, kV12CurrentRecordSize>& record,
    ClockState& state) {
    constexpr std::uint32_t kMagic = 0x32315543UL;  // "CU12"
    if (readUint32LeV12(record.data()) != kMagic || record[4] != kV12SchemaVersion ||
        record[5] != 0U || readUint16LeV12(record.data() + 6U) != kV12StatePayloadSize ||
        readUint32LeV12(record.data() + kV12CurrentRecordSize - 4U) !=
            calculateCrc32(record.data(), kV12CurrentRecordSize - 4U)) {
        return false;
    }
    std::array<std::uint8_t, kV12StatePayloadSize> payload{};
    std::copy_n(record.begin() + 8, payload.size(), payload.begin());
    return deserializeV12State(payload, state);
}

bool PersistentStateService::deserializeV12PresetRecord(
    const std::array<std::uint8_t, kV12PresetRecordSize>& record,
    char* const name,
    ClockState& state) {
    constexpr std::uint32_t kMagic = 0x32315250UL;  // "PR12"
    constexpr std::size_t kNameOffset = 8U;
    constexpr std::size_t kPayloadOffset = kNameOffset + kPresetNameLength;
    if (readUint32LeV12(record.data()) != kMagic || record[4] != kV12SchemaVersion ||
        record[5] != 1U || readUint16LeV12(record.data() + 6U) != kV12StatePayloadSize ||
        readUint32LeV12(record.data() + kV12PresetRecordSize - 4U) !=
            calculateCrc32(record.data(), kV12PresetRecordSize - 4U)) {
        return false;
    }
    for (std::size_t index = 0U; index < kPresetNameLength; ++index) {
        const char character = static_cast<char>(record[kNameOffset + index]);
        if (!isAllowedV12PresetCharacter(character)) {
            return false;
        }
        name[index] = character;
    }
    name[kPresetNameLength] = '\0';
    std::array<std::uint8_t, kV12StatePayloadSize> payload{};
    std::copy_n(
        record.begin() + static_cast<std::ptrdiff_t>(kPayloadOffset),
        payload.size(),
        payload.begin());
    return deserializeV12State(payload, state);
}

}  // namespace clockfw::services
