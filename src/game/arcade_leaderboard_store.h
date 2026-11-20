/**
 * @file arcade_leaderboard_store.h
 * @brief Compact CRC-protected Top-100 leaderboards for ranked boot Easter eggs.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hal/persistent_layout.h"
#include "hal/persistent_storage.h"

namespace clockfw::game {

/**
 * @brief Ranked arcade title; the value selects the leaderboard slot in persistent storage.
 *
 * Values are part of the persistent layout and must not be renumbered.
 */
enum class ArcadeGameId : std::uint8_t {
    PixelRaid = 0U,  ///< Pixel Raid, record magic 'P'.
    Formula1 = 1U,   ///< Formula 1, record magic 'F'.
    Breakout = 2U,   ///< Breakout, record magic 'B'.
    EggJourney = 3U  ///< Egg Journey, record magic 'E'.
};

/** @brief One ranked score with three-letter initials. */
struct LeaderboardEntry final {
    std::uint32_t score = 0U;  ///< Points; 0 never qualifies for the table.
    /** Initials 'A'..'Z' in elements 0..2, NUL-terminated. */
    std::array<char, 4U> initials{{'A', 'A', 'A', '\0'}};
};

/** @brief Top-100 table of one game, sorted by descending score. */
struct LeaderboardTable final {
    static constexpr std::size_t kMaximumEntries = 100U;      ///< Table capacity.
    std::array<LeaderboardEntry, kMaximumEntries> entries{};  ///< Valid in [0, count).
    std::uint8_t count = 0U;                                  ///< Used entries, 0..100.
};

/**
 * @brief Durable, CRC-protected Top-100 leaderboard of one ranked Easter egg.
 *
 * Each game owns one fixed record in the leaderboard region of the persistent image. Writes go
 * through hal::PersistentStorage::writeBytes(), which commits a complete new generation.
 */
class ArcadeLeaderboardStore final {
  public:
    /**
     * @brief Constructs one leaderboard view for a specific ranked Easter egg.
     * @param storage Persistent storage holding the record; must outlive the store.
     * @param gameId Game whose record this view reads and writes.
     */
    ArcadeLeaderboardStore(hal::PersistentStorage& storage, ArcadeGameId gameId);
    /**
     * @brief Loads and validates the Top-100 table, migrating a historical single score when
     *        needed.
     * @return The stored table, or a table built from the legacy single-score slot when no valid
     *         current record exists.
     */
    LeaderboardTable load() const;
    /**
     * @brief Returns the zero-based insertion rank for a score, or -1 when it does not qualify.
     * @param score Score to rank; 0 never qualifies.
     * @param table Table to rank against.
     * @return Rank 0..99, or -1.
     */
    std::int16_t qualifyingRank(std::uint32_t score, const LeaderboardTable& table) const;
    /**
     * @brief Inserts one named score in sorted order and commits the updated table atomically.
     *
     * A full table drops its last entry.
     * @param score Score to insert.
     * @param initials Initials; elements 0..2 must be 'A'..'Z'.
     * @return Rank 0..99 of the inserted entry, or -1 when the initials are invalid, the score
     *         does not qualify, or the commit failed.
     */
    [[nodiscard]] std::int16_t insertAndSave(
        std::uint32_t score,
        const std::array<char, 4U>& initials);
    /**
     * @brief Returns true after this ranked Easter egg has created its durable leaderboard
     *        record.
     * @return true when a valid current record is stored.
     */
    bool hasPersistentRecord() const;
    /**
     * @brief Creates the durable leaderboard record on first launch, preserving any legacy score.
     * @return true when a record already existed or was committed.
     */
    [[nodiscard]] bool markStarted();
    /**
     * @brief Clears the complete Top-100 table while retaining the durable started marker.
     * @return true when the empty record was committed.
     */
    [[nodiscard]] bool clear();

  private:
    /** Start of the leaderboard region in the logical persistent image, bytes. */
    static constexpr std::size_t kStorageBaseOffset =
        hal::persistent_layout::kLeaderboardRegionOffset;
    /** Header: 'A', 'R', game magic, '1', version, count, two zero bytes. */
    static constexpr std::size_t kHeaderBytes = 8U;
    /** Entry: little-endian 32-bit score, three initials, one zero byte. */
    static constexpr std::size_t kEntryBytes = 8U;
    /** Trailing little-endian CRC-32 over header and entries. */
    static constexpr std::size_t kCrcBytes = 4U;
    /** Size of one game's record, bytes. */
    static constexpr std::size_t kRecordBytes =
        kHeaderBytes + LeaderboardTable::kMaximumEntries * kEntryBytes + kCrcBytes;
    /** Number of game records in the leaderboard region. */
    static constexpr std::size_t kSlotCount = 4U;

    /**
     * @brief Returns the logical persistent-image offset assigned to this game.
     * @return Byte offset of this game's record.
     */
    std::size_t storageOffset() const;
    /**
     * @brief Returns the compact game identifier embedded in the durable record.
     * @return ASCII 'P', 'F', 'B', or 'E'.
     */
    std::uint8_t gameMagic() const;
    /**
     * @brief Reads one validated current Top-100 record without using the legacy fallback.
     *
     * Rejects a wrong header, version, count, CRC, invalid initials, or unsorted scores.
     * @param table Receives the table on success; unchanged when the read, header,
     *        or CRC check fails, partly written when a later entry check fails.
     * @return true when a valid record was read.
     */
    [[nodiscard]] bool loadCurrentRecord(LeaderboardTable& table) const;
    /**
     * @brief Reads the prerelease single-score slot as a migration fallback.
     * @return Empty table, or one entry with the legacy score when it is above 0.
     */
    LeaderboardTable loadLegacyFallback() const;
    /**
     * @brief Serializes and commits one complete validated leaderboard record.
     * @param table Table to store.
     * @return true when the persistent storage commit succeeded.
     */
    [[nodiscard]] bool save(const LeaderboardTable& table);
    /**
     * @brief Calculates reflected CRC-32 for durable leaderboard validation.
     * @param data Bytes to check.
     * @param size Number of bytes.
     * @return CRC-32 from hal::crc32().
     */
    static std::uint32_t calculateCrc32(const std::uint8_t* data, std::size_t size);

    hal::PersistentStorage& storage_;  ///< Borrowed persistent storage.
    ArcadeGameId gameId_;              ///< Game whose record is used.
};

static_assert(
    hal::persistent_layout::kLeaderboardRegionOffset + (8U + 100U * 8U + 4U) * 4U <=
        hal::PersistentStorage::kCapacityBytes,
    "Persistent storage is too small for four Top-100 leaderboards.");

}  // namespace clockfw::game
