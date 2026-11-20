/**
 * @file sequencer_pattern_store.h
 * @brief Wear-coalesced persistent Sequencer 2.0 pattern bank.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "domain/clock_types.h"
#include "domain/sequencer_pattern.h"
#include "hal/persistent_storage.h"

namespace clockfw::services {

/**
 * @brief Stores 8 patterns for each of CLOCK's 8 physical channels.
 *
 * The bank occupies the first 2 KiB of the post-V1 8..12-KiB persistence extension.
 * The remaining 2 KiB stays reserved for sparse step metadata / Song data. The bank
 * is global user data rather than being duplicated inside CURRENT/eight presets.
 */
class SequencerPatternStore final {
  public:
    /** Image offset of the bank. */
    static constexpr std::size_t kStorageOffset = hal::persistent_layout::kSequencer2RegionOffset;
    /** Bytes reserved for the bank. */
    static constexpr std::size_t kStorageBytes = hal::persistent_layout::kSequencerPatternBankBytes;
    /** Bank header bytes: magic, version, geometry, CRC-32, padding. */
    static constexpr std::size_t kHeaderBytes = 16U;
    /** Bytes of one encoded pattern record. */
    static constexpr std::size_t kRecordBytes = 24U;
    /** Number of pattern records: channels times slots per channel. */
    static constexpr std::size_t kPatternCount =
        static_cast<std::size_t>(kChannelCount) * kSequencerPatternSlotsPerChannel;

    /**
     * @brief Creates a pattern store over the shared durable persistence image.
     * @param storage Shared persistent image; not owned.
     */
    explicit SequencerPatternStore(hal::PersistentStorage& storage);

    /**
     * @brief Loads the durable bank or initializes clean in-memory defaults when
     *        absent/corrupt.
     * @return False only when the storage read failed; an absent or corrupt bank yields
     *         defaults and true.
     */
    [[nodiscard]] bool begin();

    /**
     * @brief Returns one immutable slot, or the channel 0/slot 0 pattern for invalid indices.
     * @param channelIndex Zero-based channel below kChannelCount.
     * @param slotIndex Zero-based slot below kSequencerPatternSlotsPerChannel (P1 = 0).
     * @return Reference into the store; valid for the store's lifetime.
     */
    const SequencerPatternV2& pattern(std::uint8_t channelIndex, std::uint8_t slotIndex) const;

    /**
     * @brief Returns the persisted active pattern slot for one channel, defaulting to P1.
     * @param channelIndex Zero-based channel; out-of-range values return 0.
     * @return Zero-based active slot (P1 = 0).
     */
    std::uint8_t activeSlot(std::uint8_t channelIndex) const;

    /**
     * @brief Selects P1..P8 for one channel and schedules a coalesced persistent commit.
     * @param channelIndex Zero-based channel below kChannelCount.
     * @param slotIndex Zero-based slot below kSequencerPatternSlotsPerChannel (P1 = 0).
     * @param nowMs Current time in milliseconds; unused, the central coordinator debounces.
     * @return False for invalid indices; true otherwise, also when the slot was already
     *         active (nothing marked dirty then).
     */
    [[nodiscard]] bool setActiveSlot(
        std::uint8_t channelIndex,
        std::uint8_t slotIndex,
        std::uint32_t nowMs);

    /**
     * @brief Seeds P1 of every channel from the legacy 1.x SequencerSettings.
     *
     * This is accepted only while no durable Sequencer 2.0 bank exists; slots P2..P8
     * retain their clean defaults. It provides the one-way migration bridge from the
     * historical per-channel 64-bit sequence into the new global pattern bank.
     * @param state Configuration holding the legacy per-channel sequencer settings.
     * @param nowMs Current time in milliseconds; unused, the central coordinator debounces.
     * @return False when a durable bank exists or the bank is already dirty.
     */
    [[nodiscard]] bool seedLegacyPatternOnes(const ClockState& state, std::uint32_t nowMs);

    /**
     * @brief Replaces one validated slot and schedules a coalesced persistent commit.
     * @param channelIndex Zero-based channel below kChannelCount.
     * @param slotIndex Zero-based slot below kSequencerPatternSlotsPerChannel (P1 = 0).
     * @param pattern New pattern; must pass isSequencerPatternValid(), stored clamped.
     * @param nowMs Current time in milliseconds; unused, the central coordinator debounces.
     * @return False for invalid indices or an invalid pattern.
     */
    [[nodiscard]] bool updatePattern(
        std::uint8_t channelIndex,
        std::uint8_t slotIndex,
        const SequencerPatternV2& pattern,
        std::uint32_t nowMs);

    /**
     * @brief Mutates one binary gate bit and schedules persistence.
     * @param channelIndex Zero-based channel below kChannelCount.
     * @param slotIndex Zero-based slot below kSequencerPatternSlotsPerChannel (P1 = 0).
     * @param step Zero-based step below kSequencerMaximumSteps.
     * @param enabled New gate value.
     * @param nowMs Current time in milliseconds; unused, the central coordinator debounces.
     * @return False for invalid indices.
     */
    [[nodiscard]] bool setGate(
        std::uint8_t channelIndex,
        std::uint8_t slotIndex,
        std::uint8_t step,
        bool enabled,
        std::uint32_t nowMs);

    /**
     * @brief Commits a dirty bank immediately in its own transaction (tools/tests).
     * @return True when nothing was dirty or the commit succeeded.
     */
    [[nodiscard]] bool flush();

    /**
     * @brief Stages the dirty bank into the storage update the caller opened (D-04).
     * @return True when nothing was dirty or staging succeeded.
     */
    [[nodiscard]] bool stage();

    /** @brief Marks the staged bank as durable after a successful commit. */
    void markCommitted();

    /**
     * @brief Revision counter of accepted bank mutations; drives the central debounce.
     * @return Counter that increments with every accepted mutation; wraps modulo 2^32.
     */
    std::uint32_t changeRevision() const {
        return changeRevision_;
    }

    /** @brief Restores clean defaults after ERASE ALL DATA; nothing remains pending (F-008). */
    void resetToFactory();

    /**
     * @brief Returns whether RAM differs from the durable pattern bank.
     * @return True while a mutation awaits a commit.
     */
    bool dirty() const {
        return dirty_;
    }
    /**
     * @brief Returns whether a valid Sequencer 2.0 bank was loaded or committed.
     * @return True after a successful load or commit; false after a factory reset.
     */
    bool hasDurableBank() const {
        return hasDurableBank_;
    }

  private:
    /** Bank format written by this firmware (adds the active-slot table). */
    static constexpr std::uint8_t kFormatVersion = 2U;
    /** Older bank format without the active-slot table; still accepted on load. */
    static constexpr std::uint8_t kLegacyFormatVersion = 1U;
    /** Bank offset of the per-channel active-slot table. */
    static constexpr std::size_t kActiveSlotsOffset = kHeaderBytes + kPatternCount * kRecordBytes;

    /**
     * @brief Maps channel and slot indices onto the fixed bank array.
     * @param channelIndex Zero-based channel; not range-checked here.
     * @param slotIndex Zero-based slot; not range-checked here.
     * @return Index into patterns_.
     */
    static std::size_t flatIndex(std::uint8_t channelIndex, std::uint8_t slotIndex);
    /**
     * @brief Computes the bank payload CRC-32.
     * @param data First byte of the payload.
     * @param size Number of bytes.
     * @return CRC-32 as computed by hal::crc32().
     */
    static std::uint32_t calculateCrc32(const std::uint8_t* data, std::size_t size);
    /**
     * @brief Writes one little-endian 32-bit field.
     * @param destination First of four output bytes.
     * @param value Value to write.
     */
    static void write32(std::uint8_t* destination, std::uint32_t value);
    /**
     * @brief Reads one little-endian 32-bit field.
     * @param source First of four input bytes.
     * @return Decoded value.
     */
    static std::uint32_t read32(const std::uint8_t* source);
    /**
     * @brief Encodes one pattern into its compiler-independent fixed record.
     * @param pattern Pattern to encode.
     * @param record Output of kRecordBytes bytes; bytes 20..23 are reserved (0xFF).
     */
    static void encodePattern(const SequencerPatternV2& pattern, std::uint8_t* record);
    /**
     * @brief Decodes and validates one fixed pattern record.
     * @param record Input of kRecordBytes bytes.
     * @param pattern Destination, written clamped only when true is returned.
     * @return True when the decoded pattern passes isSequencerPatternValid().
     */
    static bool decodePattern(const std::uint8_t* record, SequencerPatternV2& pattern);
    /** @brief Restores clean in-memory defaults for all 64 slots. */
    void resetDefaults();
    /**
     * @brief Decodes and validates the complete pattern bank image.
     * @param bytes Raw bank image as read from storage.
     * @return True when header, geometry, CRC-32 and every record validate; the store
     *         is changed only then.
     */
    [[nodiscard]] bool decodeBank(const std::array<std::uint8_t, kStorageBytes>& bytes);
    /**
     * @brief Encodes the complete pattern bank with header and CRC.
     * @param bytes Output image; unused bytes are 0xFF.
     */
    void encodeBank(std::array<std::uint8_t, kStorageBytes>& bytes) const;

    /** Shared persistent image; not owned. */
    hal::PersistentStorage& storage_;
    /** All patterns, indexed by flatIndex(). */
    std::array<SequencerPatternV2, kPatternCount> patterns_{};
    /** Active zero-based slot per channel. */
    std::array<std::uint8_t, kChannelCount> activeSlots_{};
    /** @brief Records one accepted mutation. */
    void markDirty();

    /** Value returned by changeRevision(). */
    std::uint32_t changeRevision_ = 0U;
    /** Value returned by dirty(). */
    bool dirty_ = false;
    /** Value returned by hasDurableBank(). */
    bool hasDurableBank_ = false;
};

static_assert(
    SequencerPatternStore::kHeaderBytes +
            SequencerPatternStore::kPatternCount * SequencerPatternStore::kRecordBytes <=
        SequencerPatternStore::kStorageBytes,
    "Sequencer pattern bank must fit inside its 2-KiB extension region.");
static_assert(
    SequencerPatternStore::kStorageOffset + SequencerPatternStore::kStorageBytes <=
        hal::PersistentStorage::kCapacityBytes,
    "Sequencer pattern bank must fit inside the current logical persistence image.");

}  // namespace clockfw::services
