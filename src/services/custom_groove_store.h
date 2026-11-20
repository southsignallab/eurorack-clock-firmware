/**
 * @file custom_groove_store.h
 * @brief Fixed-record persistent library for user-authored Custom Grooves.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "domain/custom_groove.h"
#include "hal/persistent_layout.h"
#include "hal/persistent_storage.h"

namespace clockfw::services {

/**
 * @brief Stores ten bounded Custom Groove records in the unused V1 legacy-score window.
 *
 * The store is deliberately separate from the repeated CURRENT/eight-preset ClockState
 * records. Ten 96-byte records exactly occupy bytes 3136..4095, preserving the four
 * historical 16-byte score records at 3072..3135 and the Top-100 region at 4096.
 */
class CustomGrooveStore final {
  public:
    /** Maximum characters of one stored groove name (without terminator). */
    static constexpr std::size_t kNameLength = 16U;
    /** Bytes of one record: header, name, step offsets, CRC-32. */
    static constexpr std::size_t kRecordBytes = 96U;
    /** Image offset of slot 0. */
    static constexpr std::size_t kStorageOffset = hal::persistent_layout::kCustomGrooveRegionOffset;

    /**
     * @brief Constructs the fixed-record store around the shared persistent image.
     * @param storage Shared persistent image; not owned.
     * @param deferredWrites True queues mutations in RAM until service() or a coordinator
     *                       commit; false writes every mutation to storage immediately.
     */
    explicit CustomGrooveStore(hal::PersistentStorage& storage, bool deferredWrites = false);

    /**
     * @brief Commits queued groove mutations in their own transaction when Flash writes are
     *        allowed.
     * @param allowFlashWrite False defers the commit.
     * @return False only when an attempted commit failed; the queue is then kept.
     */
    [[nodiscard]] bool service(bool allowFlashWrite);

    /**
     * @brief Reports queued slot mutations.
     * @return True while at least one slot mutation awaits a durable commit.
     */
    bool hasPendingCommit() const;

    /**
     * @brief Revision counter of accepted slot mutations.
     * @return Counter that increments with every accepted save or clear; wraps modulo 2^32.
     */
    std::uint32_t changeRevision() const {
        return changeRevision_;
    }

    /**
     * @brief Stages every queued slot into the storage update the caller opened (D-04).
     * @return False when a slot could not be staged; the queue is kept.
     */
    [[nodiscard]] bool stage();

    /** @brief Marks the staged queue as durable after a successful commit. */
    void markCommitted();

    /** @brief Drops queued mutations and cached names after ERASE ALL DATA (F-008). */
    void resetToFactory();

    /**
     * @brief Loads one validated slot, from the queue when a mutation is pending.
     * @param slotIndex Zero-based slot below kCustomGrooveSlotCount.
     * @param pattern Destination; written only when true is returned.
     * @param name Optional destination for the name, right-trimmed and null-terminated;
     *             nullptr skips the name.
     * @param nameSize Size of @p name in bytes including the terminator.
     * @return False for empty, corrupt or out-of-range slots.
     */
    [[nodiscard]] bool load(
        std::uint8_t slotIndex,
        CustomGroovePattern& pattern,
        char* name = nullptr,
        std::size_t nameSize = 0U) const;

    /**
     * @brief Accepts a named validated pattern; production stores defer Flash until safe.
     * @param slotIndex Zero-based slot below kCustomGrooveSlotCount.
     * @param name Groove name; non-printable characters become spaces, at most kNameLength
     *             characters are kept, nullptr stores an empty name.
     * @param pattern Pattern to store; must pass isCustomGroovePatternValid().
     * @return False for an invalid slot or pattern, or when an immediate write failed.
     */
    [[nodiscard]] bool save(
        std::uint8_t slotIndex,
        const char* name,
        const CustomGroovePattern& pattern);

    /**
     * @brief Accepts one slot deletion; deferred readers see it immediately.
     * @param slotIndex Zero-based slot below kCustomGrooveSlotCount.
     * @return False for an invalid slot, or when an immediate write failed.
     */
    [[nodiscard]] bool clear(std::uint8_t slotIndex);

    /**
     * @brief Reports whether a slot holds a valid record (served from the RAM cache).
     * @param slotIndex Zero-based slot; out-of-range values report false.
     * @return True when a slot contains one valid current-format record.
     */
    bool exists(std::uint8_t slotIndex) const;

    /**
     * @brief Copies a slot name or an empty string for absent/corrupt slots.
     *
     * Served from a RAM cache filled on first use and refreshed on save, clear,
     * and factory reset, so the performance screen never reads Flash per frame (F-018).
     * @param slotIndex Zero-based slot.
     * @param destination Output text buffer; nullptr is ignored.
     * @param destinationSize Output buffer size including terminator.
     */
    void name(std::uint8_t slotIndex, char* destination, std::size_t destinationSize) const;

  private:
    /**
     * @brief Calculates the record CRC over the supplied byte range.
     * @param data First byte of the range.
     * @param size Number of bytes.
     * @return CRC-32 as computed by hal::crc32().
     */
    static std::uint32_t calculateCrc32(const std::uint8_t* data, std::size_t size);
    /**
     * @brief Returns the byte offset of one fixed Custom Groove slot.
     * @param slotIndex Zero-based slot; not range-checked here.
     * @return Image offset of the slot record.
     */
    static std::size_t slotOffset(std::uint8_t slotIndex);
    /**
     * @brief Checks one character for the persistent name field.
     * @param character Character to check.
     * @return True for printable ASCII (0x20..0x7E).
     */
    static bool validNameCharacter(char character);
    /**
     * @brief Copies and sanitizes one bounded persistent Custom Groove name.
     * @param source Null-terminated name, or nullptr for an empty name.
     * @param destination Output buffer of kNameLength + 1 bytes; zero-filled first.
     */
    static void normalizeName(const char* source, char* destination);

    /**
     * @brief Fills the name/existence cache entry of one slot on first use.
     *
     * Invalidates the whole cache first when the storage content revision changed.
     * @param slotIndex Zero-based slot; out-of-range values are ignored.
     */
    void cacheSlot(std::uint8_t slotIndex) const;

    /**
     * @brief Writes a validated/erased record immediately or queues it for a safe commit.
     * @param slotIndex Zero-based slot below kCustomGrooveSlotCount.
     * @param record Complete record image.
     * @return Result of the immediate write, or true when the record was queued.
     */
    [[nodiscard]] bool writeRecord(
        std::uint8_t slotIndex,
        const std::array<std::uint8_t, kRecordBytes>& record);

    /** Shared persistent image; not owned. */
    hal::PersistentStorage& storage_;
    /** True queues mutations instead of writing immediately. */
    bool deferredWrites_ = false;
    /** Queued record image per slot. */
    std::array<std::array<std::uint8_t, kRecordBytes>, kCustomGrooveSlotCount> queuedRecords_{};
    /** True for each slot with a queued mutation. */
    std::array<bool, kCustomGrooveSlotCount> queuedSlots_{};
    /** Value returned by changeRevision(). */
    std::uint32_t changeRevision_ = 0U;

    /** @brief Cached name and existence of one slot. */
    struct SlotCache final {
        /** True when exists and name reflect the current storage content. */
        bool cached = false;
        /** True when the slot holds a valid record. */
        bool exists = false;
        /** Right-trimmed name; empty when the slot is absent. */
        std::array<char, kNameLength + 1U> name{};
    };
    /** Per-slot name and existence cache, filled lazily by cacheSlot(). */
    mutable std::array<SlotCache, kCustomGrooveSlotCount> slotCache_{};
    /** Flash content revision the cache was filled against; any other writer invalidates it. */
    mutable std::uint32_t cacheContentRevision_ = 0U;
};

static_assert(
    CustomGrooveStore::kStorageOffset +
            static_cast<std::size_t>(kCustomGrooveSlotCount) * CustomGrooveStore::kRecordBytes ==
        hal::persistent_layout::kLeaderboardRegionOffset,
    "Custom Groove records must end exactly at the Top-100 region boundary.");

}  // namespace clockfw::services
