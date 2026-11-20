/**
 * @file sequencer_step_store.h
 * @brief Sparse persistent Sequencer 2.0 per-step expression store.
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
#include "domain/sequencer_step_metadata.h"
#include "hal/persistent_storage.h"

namespace clockfw::services {

/**
 * @brief Stores only exceptional per-step expression records across all 64 patterns.
 *
 * Each persistent record is exactly four bytes: a compiler-independent 13-bit
 * channel/slot/step key plus one stable 16-bit metadata word. Untouched steps
 * consume no record. The last 512 bytes of the 12-KiB image remain reserved for
 * the later Song Mode rather than being consumed by step expression.
 */
class SequencerStepStore final {
  public:
    /** Image offset of the sparse store. */
    static constexpr std::size_t kStorageOffset =
        hal::persistent_layout::kSequencerStepMetadataOffset;
    /** Bytes reserved for the sparse store. */
    static constexpr std::size_t kStorageBytes =
        hal::persistent_layout::kSequencerStepMetadataBytes;
    /** Header bytes: magic, version, record count, step count, CRC-32, padding. */
    static constexpr std::size_t kHeaderBytes = 16U;
    /** Bytes of one record: 16-bit key plus 16-bit metadata word. */
    static constexpr std::size_t kRecordBytes = 4U;
    /** Maximum number of sparse records that fit after the header. */
    static constexpr std::size_t kMaximumOverrides = (kStorageBytes - kHeaderBytes) / kRecordBytes;

    /**
     * @brief Creates the sparse store over the shared A/B persistence image.
     * @param storage Shared persistent image; not owned.
     */
    explicit SequencerStepStore(hal::PersistentStorage& storage);

    /**
     * @brief Loads a valid sparse image or initializes an empty in-memory store.
     * @return False only when the storage read failed; an erased or corrupt image yields
     *         an empty store and true.
     */
    [[nodiscard]] bool begin();

    /**
     * @brief Returns one override, or DEFAULT metadata when no explicit record exists.
     * @param channelIndex Zero-based channel below kChannelCount.
     * @param slotIndex Zero-based pattern slot below kSequencerPatternSlotsPerChannel.
     * @param step Zero-based step below kSequencerMaximumSteps.
     * @return Stored metadata; DEFAULT also for out-of-range indices.
     */
    SequencerStepMetadata metadata(
        std::uint8_t channelIndex,
        std::uint8_t slotIndex,
        std::uint8_t step) const;

    /**
     * @brief Expands one active pattern into the fixed 128-word scheduler snapshot.
     * @param channelIndex Zero-based channel below kChannelCount.
     * @param slotIndex Zero-based pattern slot below kSequencerPatternSlotsPerChannel.
     * @param destination Receives one packed word per step; 0 (DEFAULT) where no record
     *                    exists, all 0 for out-of-range indices.
     */
    void loadPatternWords(
        std::uint8_t channelIndex,
        std::uint8_t slotIndex,
        std::array<SequencerStepMetadataWord, kSequencerMaximumSteps>& destination) const;

    /**
     * @brief Adds, replaces, or removes one sparse override and schedules persistence.
     * @param channelIndex Zero-based channel below kChannelCount.
     * @param slotIndex Zero-based pattern slot below kSequencerPatternSlotsPerChannel.
     * @param step Zero-based step below kSequencerMaximumSteps.
     * @param metadata New metadata; DEFAULT removes the record.
     * @param nowMs Current time in milliseconds; unused, the central coordinator debounces.
     * @return False for invalid indices or metadata, or when the store is full.
     */
    [[nodiscard]] bool updateMetadata(
        std::uint8_t channelIndex,
        std::uint8_t slotIndex,
        std::uint8_t step,
        const SequencerStepMetadata& metadata,
        std::uint32_t nowMs);

    /**
     * @brief Removes every expression override owned by one pattern slot.
     * @param channelIndex Zero-based channel below kChannelCount.
     * @param slotIndex Zero-based pattern slot below kSequencerPatternSlotsPerChannel.
     * @param nowMs Current time in milliseconds; unused, the central coordinator debounces.
     * @return False for invalid indices.
     */
    [[nodiscard]] bool clearPattern(
        std::uint8_t channelIndex,
        std::uint8_t slotIndex,
        std::uint32_t nowMs);

    /**
     * @brief Replaces one pattern's overrides from a complete 128-word snapshot.
     * @param channelIndex Zero-based channel below kChannelCount.
     * @param slotIndex Zero-based pattern slot below kSequencerPatternSlotsPerChannel.
     * @param words Packed metadata word per step; 0 (DEFAULT) means no record.
     * @param nowMs Current time in milliseconds; unused, the central coordinator debounces.
     * @return False for invalid indices, a non-canonical word, insufficient capacity, or
     *         a word that updateMetadata() rejects (the pattern may then be partly
     *         replaced).
     */
    [[nodiscard]] bool replacePatternWords(
        std::uint8_t channelIndex,
        std::uint8_t slotIndex,
        const std::array<SequencerStepMetadataWord, kSequencerMaximumSteps>& words,
        std::uint32_t nowMs);

    /**
     * @brief Forces an immediate durable commit of pending step expression in its own
     *        transaction.
     * @return True when nothing was dirty or the commit succeeded.
     */
    [[nodiscard]] bool flush();

    /**
     * @brief Stages the dirty sparse image into the storage update the caller opened (D-04).
     * @return True when nothing was dirty or staging succeeded.
     */
    [[nodiscard]] bool stage();

    /** @brief Marks the staged image as durable after a successful commit. */
    void markCommitted();

    /**
     * @brief Revision counter of accepted mutations; drives the central debounce.
     * @return Counter that increments with every accepted mutation; wraps modulo 2^32.
     */
    std::uint32_t changeRevision() const {
        return changeRevision_;
    }

    /**
     * @brief Restores the all-DEFAULT image after ERASE ALL DATA; nothing remains pending
     *        (F-008).
     */
    void resetToFactory();

    /**
     * @brief Returns the number of non-default sparse records currently held in RAM.
     * @return Record count, at most kMaximumOverrides.
     */
    std::size_t overrideCount() const {
        return count_;
    }

    /**
     * @brief Returns whether RAM differs from the durable sparse image.
     * @return True while a mutation awaits a commit.
     */
    bool dirty() const {
        return dirty_;
    }

  private:
    /** Sparse image format version. */
    static constexpr std::uint8_t kFormatVersion = 1U;
    /** Key value of an unused RAM entry. */
    static constexpr std::uint16_t kUnusedKey = 0xFFFFU;

    /**
     * @brief Builds the stable 13-bit channel/slot/step record key.
     *
     * Bits 0..6 hold the step, bits 7..9 the channel, bits 10..12 the slot.
     * @param channelIndex Zero-based channel; not range-checked here.
     * @param slotIndex Zero-based slot; not range-checked here.
     * @param step Zero-based step; not range-checked here.
     * @return Record key.
     */
    static std::uint16_t makeKey(
        std::uint8_t channelIndex,
        std::uint8_t slotIndex,
        std::uint8_t step);
    /**
     * @brief Decodes and validates one stable record key.
     * @param key Record key as built by makeKey().
     * @param channelIndex Receives the channel.
     * @param slotIndex Receives the slot.
     * @param step Receives the step.
     * @return False when reserved bits are set or a field is out of range.
     */
    static bool decodeKey(
        std::uint16_t key,
        std::uint8_t& channelIndex,
        std::uint8_t& slotIndex,
        std::uint8_t& step);
    /**
     * @brief Computes the payload CRC-32.
     * @param data First byte of the payload.
     * @param size Number of bytes.
     * @return CRC-32 as computed by hal::crc32().
     */
    static std::uint32_t calculateCrc32(const std::uint8_t* data, std::size_t size);
    /**
     * @brief Writes one little-endian 16-bit field.
     * @param destination First of two output bytes.
     * @param value Value to write.
     */
    static void write16(std::uint8_t* destination, std::uint16_t value);
    /**
     * @brief Reads one little-endian 16-bit field.
     * @param source First of two input bytes.
     * @return Decoded value.
     */
    static std::uint16_t read16(const std::uint8_t* source);
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
     * @brief Finds one record index by linear search over the held records.
     * @param key Record key.
     * @return Index into keys_/words_, or kMaximumOverrides when absent.
     */
    std::size_t findKey(std::uint16_t key) const;
    /**
     * @brief Records one accepted mutation; commit timing is owned by
     *        PersistenceCoordinator.
     * @param nowMs Current time in milliseconds; unused.
     */
    void markDirty(std::uint32_t nowMs);
    /** @brief Restores an empty sparse store. */
    void resetDefaults();
    /**
     * @brief Decodes and validates the complete sparse store image.
     * @param bytes Raw image as read from storage.
     * @return True when header, CRC-32 and every record (valid key, canonical non-default
     *         word, no duplicate) validate; the store is changed only then.
     */
    [[nodiscard]] bool decodeStore(const std::array<std::uint8_t, kStorageBytes>& bytes);
    /**
     * @brief Encodes the complete sparse store image with CRC.
     * @param bytes Output image; unused bytes are 0xFF.
     */
    void encodeStore(std::array<std::uint8_t, kStorageBytes>& bytes) const;

    /** Shared persistent image; not owned. */
    hal::PersistentStorage& storage_;
    /** Record keys; entries from count_ on are kUnusedKey. */
    std::array<std::uint16_t, kMaximumOverrides> keys_{};
    /** Packed metadata word per record. */
    std::array<SequencerStepMetadataWord, kMaximumOverrides> words_{};
    /** Number of held records. */
    std::size_t count_ = 0U;
    /** Value returned by changeRevision(). */
    std::uint32_t changeRevision_ = 0U;
    /** Value returned by dirty(). */
    bool dirty_ = false;
};

static_assert(
    SequencerStepStore::kStorageOffset + SequencerStepStore::kStorageBytes <=
        hal::persistent_layout::kSequencerSongRegionOffset,
    "Step metadata must not consume the reserved Song Mode region.");
static_assert(
    SequencerStepStore::kMaximumOverrides == 380U,
    "Sparse step metadata capacity changed unexpectedly.");

}  // namespace clockfw::services
