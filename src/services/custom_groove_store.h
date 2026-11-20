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
    static constexpr std::size_t kNameLength = 16U;
    static constexpr std::size_t kRecordBytes = 96U;
    static constexpr std::size_t kStorageOffset = hal::persistent_layout::kCustomGrooveRegionOffset;

    /** @brief Constructs the fixed-record store around the shared persistent image. */
    explicit CustomGrooveStore(hal::PersistentStorage& storage, bool deferredWrites = false);

    /** @brief Commits queued groove mutations in their own transaction when Flash writes are allowed. */
    [[nodiscard]] bool service(bool allowFlashWrite);

    /** @brief True while at least one slot mutation awaits a durable commit. */
    bool hasPendingCommit() const;

    /** @brief Increments with every accepted slot mutation (save or clear). */
    std::uint32_t changeRevision() const {
        return changeRevision_;
    }

    /** @brief Stages every queued slot into the storage update the caller opened (D-04). */
    [[nodiscard]] bool stage();

    /** @brief Marks the staged queue as durable after a successful commit. */
    void markCommitted();

    /** @brief Drops queued mutations and cached names after ERASE ALL DATA (F-008). */
    void resetToFactory();

    /** @brief Loads one validated slot; returns false for empty/corrupt/out-of-range slots. */
    [[nodiscard]] bool load(
        std::uint8_t slotIndex,
        CustomGroovePattern& pattern,
        char* name = nullptr,
        std::size_t nameSize = 0U) const;

    /** @brief Accepts a named validated pattern; production stores defer Flash until safe. */
    [[nodiscard]] bool save(
        std::uint8_t slotIndex,
        const char* name,
        const CustomGroovePattern& pattern);

    /** @brief Accepts one slot deletion; deferred readers see it immediately. */
    [[nodiscard]] bool clear(std::uint8_t slotIndex);

    /** @brief Returns true when a slot contains one valid current-format record. */
    bool exists(std::uint8_t slotIndex) const;

    /**
     * @brief Copies a slot name or an empty string for absent/corrupt slots.
     *
     * Served from a RAM cache filled on first use and refreshed on save, clear,
     * and factory reset, so the performance screen never reads Flash per frame (F-018).
     */
    void name(std::uint8_t slotIndex, char* destination, std::size_t destinationSize) const;

  private:
    /** @brief Calculates the record CRC over the supplied byte range. */
    static std::uint32_t calculateCrc32(const std::uint8_t* data, std::size_t size);
    /** @brief Returns the byte offset of one fixed Custom Groove slot. */
    static std::size_t slotOffset(std::uint8_t slotIndex);
    /** @brief Returns true for printable ASCII accepted by the persistent name field. */
    static bool validNameCharacter(char character);
    /** @brief Copies and sanitizes one bounded persistent Custom Groove name. */
    static void normalizeName(const char* source, char* destination);

    /** @brief Fills the name/existence cache entry of one slot on first use. */
    void cacheSlot(std::uint8_t slotIndex) const;

    /** @brief Writes a validated/erased record immediately or queues it for a safe commit. */
    [[nodiscard]] bool writeRecord(
        std::uint8_t slotIndex,
        const std::array<std::uint8_t, kRecordBytes>& record);

    hal::PersistentStorage& storage_;
    bool deferredWrites_ = false;
    std::array<std::array<std::uint8_t, kRecordBytes>, kCustomGrooveSlotCount> queuedRecords_{};
    std::array<bool, kCustomGrooveSlotCount> queuedSlots_{};
    std::uint32_t changeRevision_ = 0U;

    /** @brief Cached name and existence of one slot. */
    struct SlotCache final {
        bool cached = false;
        bool exists = false;
        std::array<char, kNameLength + 1U> name{};
    };
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
