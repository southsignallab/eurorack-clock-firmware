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

/** Value of an erased Flash byte; also the fill of unwritten image bytes. */
inline constexpr std::uint8_t kErasedByte = 0xFFU;
/** Number of A/B slots. */
inline constexpr std::size_t kSlotCount = 2U;
/** Header magic word. */
inline constexpr std::uint32_t kSlotMagic = 0x434C4B50UL;  // "CLKP"
/** Slot format this firmware reads and writes; other values classify as Foreign. */
inline constexpr std::uint32_t kSlotFormatVersion = 1U;
/** Value of the COMMIT word of a complete slot. */
inline constexpr std::uint32_t kCommitMarker = 0xC10C17EDUL;
/** Size of SlotHeader in bytes. */
inline constexpr std::size_t kSlotHeaderBytes = 32U;
/** Number of 32-bit words in SlotHeader. */
inline constexpr std::size_t kHeaderWordCount = kSlotHeaderBytes / sizeof(std::uint32_t);
/** Index of the COMMIT word, the last header word. */
inline constexpr std::size_t kCommitWordIndex = kHeaderWordCount - 1U;
/** Byte offset of the payload inside a slot, directly after the header. */
inline constexpr std::size_t kPayloadOffset = kSlotHeaderBytes;
/** Payload size of a legacy 4-KiB image, still read for migration. */
inline constexpr std::size_t kLegacyPayloadBytes = 4096U;
/** Payload size of a historical 8-KiB image, still read for migration. */
inline constexpr std::size_t kV1PayloadBytes = persistent_layout::kV1ImageBytes;

/** @brief Raw 32-byte slot header; COMMIT is the last word and is programmed last. */
struct SlotHeader final {
    /** kSlotMagic in a written slot. */
    std::uint32_t magic = kSlotMagic;
    /** Slot format version. */
    std::uint32_t formatVersion = kSlotFormatVersion;
    /** Commit generation; the newer of two slots wins by wrapping signed difference. */
    std::uint32_t generation = 0U;
    /** Payload size in bytes. */
    std::uint32_t payloadSize = static_cast<std::uint32_t>(PersistentStorage::kCapacityBytes);
    /** crc32() of the payload. */
    std::uint32_t payloadCrc32 = 0U;
    /** headerCrc32() of the first five words. */
    std::uint32_t headerCrc32 = 0U;
    /** Unused; left erased. */
    std::uint32_t reserved = 0xFFFFFFFFUL;
    /** kCommitMarker once the slot is complete; erased before. */
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
    /** Classification per slot index. */
    std::array<SlotKind, kSlotCount> kinds{{SlotKind::Empty, SlotKind::Empty}};
    /** Header as read from each slot, also for Empty slots. */
    std::array<SlotHeader, kSlotCount> headers{};
    int active = -1;           ///< Newest Valid slot, or -1.
    int newestCommitted = -1;  ///< Newest Valid or Foreign slot, or -1.

    /**
     * @brief True while the newest committed generation is in a format this firmware
     * cannot write over.
     * @return True when newestCommitted is a Foreign slot.
     */
    bool writeLocked() const {
        return newestCommitted >= 0 &&
            kinds[static_cast<std::size_t>(newestCommitted)] == SlotKind::Foreign;
    }
};

/**
 * @brief CRC over the five immutable header words.
 * @param header Header to check; headerCrc32, reserved and commitMarker are not covered.
 * @return crc32() of magic, formatVersion, generation, payloadSize and payloadCrc32.
 */
std::uint32_t headerCrc32(const SlotHeader& header);

/**
 * @brief Builds a sealed header (CRCs and COMMIT set) for one payload.
 * @param generation Generation to record.
 * @param payload Payload bytes; read for the CRC, not retained.
 * @param payloadSize Payload size in bytes.
 * @return Header with current magic and format version, both CRCs and kCommitMarker.
 */
SlotHeader sealedHeader(
    std::uint32_t generation,
    const std::uint8_t* payload,
    std::size_t payloadSize);

/**
 * @brief Memory-mapped (target) or simulated (host) base address of one slot.
 * @param slotIndex Slot index, below kSlotCount; not range-checked.
 * @return First byte of the slot header.
 */
const std::uint8_t* slotBase(std::size_t slotIndex);

/**
 * @brief Returns the cached scan, validating both slots only when the cache is stale.
 * @return Reference to the cache; valid until the next invalidateScan() or commitImage().
 */
const SlotScan& scan();

/** @brief Drops the cached scan; called before every physical slot modification. */
void invalidateScan();

/**
 * @brief Increments on every invalidation, i.e. whenever slot content may have changed.
 * @return Revision counter, wrapping at 2^32.
 */
std::uint32_t contentRevision();

/**
 * @brief Erases, programs and verifies one slot; COMMIT is written last.
 * @param targetSlot Slot index to overwrite, below kSlotCount.
 * @param generation Generation to record in the header.
 * @param image Complete logical image to write as payload.
 * @return True when the slot verifies as the newest Valid slot with @p generation.
 *
 * Blocking: erases one 16-KiB sector and programs it word by word; the caller holds
 * the commit gate. The scan cache is replaced by the verified result.
 */
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
