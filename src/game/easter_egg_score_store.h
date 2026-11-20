/**
 * @file easter_egg_score_store.h
 * @brief CRC-protected persistent high scores for boot Easter eggs.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <array>
#include <cstdint>

#include "hal/persistent_layout.h"
#include "hal/persistent_storage.h"

namespace clockfw::game {

/** @brief Identifies the independent durable score slot used by one Easter egg. */
enum class EasterEggScoreId : std::uint8_t {
    PixelRaid = 0U,  ///< Pixel Raid slot, magic byte 'P'.
    Formula1 = 1U,   ///< Formula 1 slot, magic byte 'F'.
    Breakout = 2U,   ///< Breakout slot, magic byte 'B'.
    EggJourney = 3U  ///< Egg Journey slot, prerelease magic byte 'M'.
};

/** @brief Persistent three-letter arcade high-score entry. */
struct HighScoreEntry {
    std::uint32_t score = 0U;  ///< Points.
    /** Initials 'A'..'Z' in elements 0..2, NUL-terminated. */
    std::array<char, 4U> initials{{'A', 'A', 'A', '\0'}};
};

/** @brief Stores one Easter-egg high score in its own dedicated NVM slot. */
class EasterEggScoreStore final {
  public:
    /**
     * @brief Constructs a store for the selected game; Pixel Raid remains the compatibility
     *        default.
     * @param storage Persistent storage holding the slot; must outlive the store.
     * @param scoreId Slot to read and write.
     */
    explicit EasterEggScoreStore(
        hal::PersistentStorage& storage,
        EasterEggScoreId scoreId = EasterEggScoreId::PixelRaid);

    /**
     * @brief Loads a validated score record, returning the default zero score when absent.
     * @return Stored entry, or a default entry (score 0, "AAA") when the read, header, CRC, or
     *         initials check fails.
     */
    HighScoreEntry load() const;

    /**
     * @brief Persists a validated score entry immediately. Safe because boot games run before
     *        clocks.
     * @param entry Entry to store; initials 0..2 must be 'A'..'Z'.
     * @return true when the initials are valid and the persistent storage commit succeeded.
     */
    [[nodiscard]] bool save(const HighScoreEntry& entry);

  private:
    /** Start of the legacy score region in the logical persistent image, bytes. */
    static constexpr std::size_t kStorageBaseOffset =
        hal::persistent_layout::kLegacyScoreRegionOffset;
    /**
     * Record: magic byte, 'R', 'H', 'S', version, little-endian 32-bit score, three initials,
     * little-endian CRC-32 over bytes 0..11.
     */
    static constexpr std::size_t kRecordSize = 16U;
    /** Number of score slots in the legacy region. */
    static constexpr std::size_t kSlotCount = 4U;
    static_assert(
        kRecordSize * kSlotCount == hal::persistent_layout::kLegacyScoreRegionBytes,
        "Legacy score records must fill the legacy score region exactly.");

    /**
     * @brief Calculates the standard reflected CRC-32 used throughout firmware persistence.
     * @param data Bytes to check.
     * @param size Number of bytes.
     * @return CRC-32 from hal::crc32().
     */
    static std::uint32_t calculateCrc32(const std::uint8_t* data, std::size_t size);
    /**
     * @brief Returns the first magic byte identifying the configured score slot.
     * @return ASCII 'P', 'F', 'B', or 'M'.
     */
    std::uint8_t magicGameByte() const;
    /**
     * @brief Returns the logical storage offset assigned to the configured score slot.
     * @return Byte offset of the slot's record.
     */
    std::size_t storageOffset() const;

    hal::PersistentStorage& storage_;  ///< Borrowed persistent storage.
    EasterEggScoreId scoreId_;         ///< Slot used by this store.
};

}  // namespace clockfw::game
