/**
 * @file persistent_storage_slots.h
 * @brief Internal A/B slot format, cached slot scan, and physical commit of PersistentStorage.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * Only persistent_storage.cpp includes this header. It is split out so the
 * public storage API and the slot format each stay inside the per-file size limit.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hal/persistent_storage.h"

namespace clockfw::hal::persistent_slots {

inline constexpr std::uint8_t kErasedByte = 0xFFU;
inline constexpr std::size_t kSlotCount = 2U;
inline constexpr std::uint32_t kSlotMagic = 0x434C4B50UL;  // "CLKP"
inline constexpr std::uint32_t kSlotFormatVersion = 1U;
inline constexpr std::uint32_t kCommitMarker = 0xC10C17EDUL;
inline constexpr std::size_t kSlotHeaderBytes = 32U;
inline constexpr std::size_t kHeaderWordCount = kSlotHeaderBytes / sizeof(std::uint32_t);
inline constexpr std::size_t kCommitWordIndex = kHeaderWordCount - 1U;
inline constexpr std::size_t kPayloadOffset = kSlotHeaderBytes;
inline constexpr std::size_t kLegacyPayloadBytes = 4096U;
inline constexpr std::size_t kV1PayloadBytes = persistent_layout::kV1ImageBytes;

/** @brief Raw 32-byte slot header; COMMIT is the last word and is programmed last. */
struct SlotHeader final {
    std::uint32_t magic = kSlotMagic;
    std::uint32_t formatVersion = kSlotFormatVersion;
    std::uint32_t generation = 0U;
    std::uint32_t payloadSize = static_cast<std::uint32_t>(PersistentStorage::kCapacityBytes);
    std::uint32_t payloadCrc32 = 0U;
    std::uint32_t headerCrc32 = 0U;
    std::uint32_t reserved = 0xFFFFFFFFUL;
    std::uint32_t commitMarker = 0xFFFFFFFFUL;
};
static_assert(sizeof(SlotHeader) == kSlotHeaderBytes, "Unexpected slot-header padding");

/** @brief Classification of one physical slot. */
enum class SlotKind : std::uint8_t {
    Empty,  ///< Erased, torn, or corrupt; never preferred.
    Valid,  ///< Committed slot in a format and size this firmware reads.
    Foreign,  ///< Committed with intact header in a format or size this firmware does not know, see F-045.
};

/** @brief Result of validating both slots once; cached until the next physical commit. */
struct SlotScan final {
    std::array<SlotKind, kSlotCount> kinds{{SlotKind::Empty, SlotKind::Empty}};
    std::array<SlotHeader, kSlotCount> headers{};
    int active = -1;           ///< Newest Valid slot, or -1.
    int newestCommitted = -1;  ///< Newest Valid or Foreign slot, or -1.

    /** @brief True while the newest committed generation is in a format this firmware cannot write over. */
    bool writeLocked() const {
        return newestCommitted >= 0 &&
            kinds[static_cast<std::size_t>(newestCommitted)] == SlotKind::Foreign;
    }
};

/** @brief CRC over the five immutable header words. */
std::uint32_t headerCrc32(const SlotHeader& header);

/** @brief Builds a sealed header (CRCs and COMMIT set) for one payload. */
SlotHeader sealedHeader(
    std::uint32_t generation,
    const std::uint8_t* payload,
    std::size_t payloadSize);

/** @brief Memory-mapped (target) or simulated (host) base address of one slot. */
const std::uint8_t* slotBase(std::size_t slotIndex);

/** @brief Returns the cached scan, validating both slots only when the cache is stale. */
const SlotScan& scan();

/** @brief Drops the cached scan; called before every physical slot modification. */
void invalidateScan();

/** @brief Increments on every invalidation, i.e. whenever slot content may have changed. */
std::uint32_t contentRevision();

/** @brief Erases, programs and verifies one slot; COMMIT is written last. */
bool commitImage(
    std::size_t targetSlot,
    std::uint32_t generation,
    const std::array<std::uint8_t, PersistentStorage::kCapacityBytes>& image);

#ifdef CLOCK_HOST_TEST
/** @brief Deterministic fault injection and counters of the simulated Flash. */
struct HostFaults final {
    bool failNextRead = false;
    bool failNextWrite = false;
    bool powerLossBeforeCommit = false;
    bool failAllWrites = false;
    std::uint32_t commitCount = 0U;
    std::uint32_t commitAttempts = 0U;
    std::uint32_t payloadValidations = 0U;
    std::uint32_t reads = 0U;
    /** @brief Called at the start of every readBytes(). */
    void (*readHook)() = nullptr;
};

/** @brief Returns the host fault/counter block. */
HostFaults& hostFaults();

/** @brief Writable view of one simulated slot; callers must invalidateScan() afterwards. */
std::uint8_t* mutableSlotBase(std::size_t slotIndex);

/** @brief Erases both simulated slots. */
void eraseHostSlots();
#endif

}  // namespace clockfw::hal::persistent_slots
