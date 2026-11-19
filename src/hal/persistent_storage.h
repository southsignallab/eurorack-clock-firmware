/**
 * @file persistent_storage.h
 * @brief Power-loss-safe A/B Flash persistence boundary for small firmware settings.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hal/persistent_layout.h"

namespace clockfw::hal {

/**
 * @brief Interlock between a physical Flash commit and the real-time scheduler (D-04).
 *
 * STM32F4 stalls instruction fetch while a sector is erased or programmed. The
 * gate is asked immediately before the erase; it must refuse while the engine
 * runs and, once open, defer every scheduler-initiated start until close().
 */
class FlashCommitGate {
public:
    /** @brief Atomically checks the engine and opens the commit window; false keeps Flash untouched. */
    [[nodiscard]] virtual bool tryOpenCommitWindow() = 0;
    /** @brief Closes the window and executes any start deferred while it was open. */
    virtual void closeCommitWindow() = 0;

protected:
    /** @brief Non-virtual by design: gates are never deleted through this interface. */
    ~FlashCommitGate() = default;
};

/** @brief Outcome of the most recent commitUpdate()/writeBytes() call. */
enum class CommitOutcome : std::uint8_t {
    None,       ///< No commit attempted yet.
    Committed,  ///< A new generation is durable and verified.
    Unchanged,  ///< Staged image equals the active one; Flash untouched.
    Deferred,   ///< The commit gate refused because the engine runs; retry later, not an error.
    Locked,     ///< Newest slot is in an unknown, newer format; writes are blocked, see F-045.
    Failed,     ///< Erase, program, or verify failed; the previous generation remains.
};

/**
 * @brief Provides a small byte-addressed persistence API backed by two independent
 * STM32F401 16-KiB Flash erase sectors.
 *
 * Every physical commit writes a complete logical image to the inactive slot,
 * verifies it, and programs the commit marker last. The previously committed
 * slot is therefore never erased until a newer complete image is durable.
 */
class PersistentStorage final {
public:
    /** Current logical image size. Expanded to the 12-KiB policy ceiling for Sequencer 2.0. */
    static constexpr std::size_t kCapacityBytes = persistent_layout::kCurrentImageBytes;

    /** Hard architectural ceiling for future persistent images inside one 16-KiB slot. */
    static constexpr std::size_t kMaximumImageBytes = persistent_layout::kMaximumImageBytes;

    /** Physical size of each independently erasable STM32F401 slot sector. */
    static constexpr std::size_t kPhysicalSlotBytes = 16U * 1024U;

    static_assert(kCapacityBytes <= kMaximumImageBytes, "Persistent image exceeds 12-KiB policy");

    /**
     * @brief Reads one range from the newest valid committed logical image.
     *
     * Slot validation (two payload CRCs) runs once per boot and after each
     * commit; every further read is a bounded memcpy (F-009).
     * @param offset Byte offset inside the logical image.
     * @param destination Destination buffer.
     * @param size Number of bytes to read.
     * @return True when the requested range was valid and readable.
     */
    [[nodiscard]] bool readBytes(std::size_t offset, std::uint8_t* destination, std::size_t size) const;
    /**
     * @brief Updates one logical range and commits a complete new A/B generation.
     * @param offset Byte offset inside the logical image.
     * @param source Source buffer.
     * @param size Number of bytes to copy.
     * @return True only after the new generation has been committed and verified.
     */
    [[nodiscard]] bool writeBytes(std::size_t offset, const std::uint8_t* source, std::size_t size);

    /**
     * @brief Starts one whole-image update in bounded static storage.
     *
     * The staging image lives inside PersistentStorage rather than on the call stack.
     * This is used when several logical records must share one physical Flash commit.
     */
    [[nodiscard]] bool beginUpdate();

    /**
     * @brief Starts a whole-image replacement from an erased staging image.
     *
     * The only path that may overwrite a slot in an unknown format (F-045). It is
     * reserved for the guarded ERASE ALL DATA flow; the caller stages every byte
     * it wants to keep. The commit always writes, even when the staged image
     * happens to equal the active one, so the lock is cleared durably.
     */
    [[nodiscard]] bool beginFullReplacement();

    /** @brief Applies one byte range to the currently staged image. */
    [[nodiscard]] bool stageBytes(std::size_t offset, const std::uint8_t* source, std::size_t size);

    /** @brief Fills one byte range in the currently staged image. */
    [[nodiscard]] bool stageFill(std::size_t offset, std::size_t size, std::uint8_t value);

    /** @brief Commits the staged image atomically to the inactive A/B Flash slot. */
    [[nodiscard]] bool commitUpdate();

    /** @brief Discards a staged image without touching Flash. */
    void cancelUpdate();

    /** @brief True while the newest committed slot is in a format this firmware must not overwrite. */
    bool writeLocked() const;

    /** @brief Outcome of the most recent commit attempt through this instance. */
    CommitOutcome lastCommitOutcome() const { return lastOutcome_; }

    /**
     * @brief Changes whenever Flash content may have changed (commit, test seed).
     *
     * Lets RAM caches of stored records stay coherent with every writer of the
     * shared image without re-reading Flash while it is unchanged.
     */
    static std::uint32_t contentRevision();

    /** @brief Installs the scheduler interlock consulted before every physical commit. */
    void setCommitGate(FlashCommitGate* gate) { commitGate_ = gate; }

#ifdef CLOCK_SIMULATOR
    /**
     * @brief Copies the currently staged image without committing it (simulator hosts).
     * @param destination Buffer of kCapacityBytes bytes.
     * @return False when no update is open.
     */
    bool copyStagedImageForSimulator(std::array<std::uint8_t, kCapacityBytes>& destination) const;
#endif

#ifdef CLOCK_HOST_TEST
    /** @brief Resets both simulated Flash sectors to the erased value 0xFF. */
    static void resetForTest();

    /** @brief Returns number of successfully committed A/B slot generations. */
    static std::uint32_t writeCommitCountForTest();

    /** @brief Makes the next logical read fail without touching storage. */
    static void failNextReadForTest();

    /** @brief Calls @p hook at the start of every readBytes(); nullptr removes it. */
    static void setReadHookForTest(void (*hook)());

    /** @brief Makes the next write fail before touching the inactive slot. */
    static void failNextWriteForTest();

    /** @brief Makes every write fail until cleared, like a worn-out sector (F-021 tests). */
    static void failAllWritesForTest(bool fail);

    /** @brief Number of commits that reached the physical stage (successful or failed). */
    static std::uint32_t commitAttemptCountForTest();

    /** @brief Simulates power loss after erase/payload programming but before COMMIT. */
    static void powerLossBeforeCommitForTest();

    /** @brief Corrupts one byte in the newest committed slot to exercise CRC fallback. */
    static void corruptNewestPayloadByteForTest(std::size_t offset);

    /** @brief Replaces one raw header word in the newest committed slot for validator tests. */
    static void corruptNewestHeaderWordForTest(std::size_t wordIndex, std::uint32_t value);

    /** @brief Seeds one valid legacy 4-KiB committed slot for migration regression tests. */
    static bool seedLegacyImageForTest(const std::uint8_t* image, std::size_t size);

    /** @brief Seeds one valid historical 8-KiB committed slot for extension-migration tests. */
    static bool seedV1ImageForTest(const std::uint8_t* image, std::size_t size);

    /** @brief Writes a committed slot with an intact header in a foreign format (F-045 tests). */
    static void seedForeignSlotForTest(
        std::size_t slotIndex,
        std::uint32_t formatVersion,
        std::uint32_t generation,
        bool intactHeaderCrc = true,
        std::uint32_t payloadSize = kCapacityBytes);

    /** @brief Forgets the cached slot scan and zeroes the counters, like a cold boot. */
    static void simulateRebootForTest();

    /** @brief Number of full-payload CRC validations since the last reset/reboot. */
    static std::uint32_t payloadValidationCountForTest();

    /** @brief Number of readBytes() calls since the last reset/reboot. */
    static std::uint32_t readCountForTest();
#endif

private:
    /** @brief Returns whether one byte range fits completely inside the logical image. */
    static bool isRangeValid(std::size_t offset, std::size_t size);

    // Deliberately permanent scratch storage: the 12-KiB BSS allocation is predictable
    // and avoids multi-kilobyte stack frames during whole-image A/B commits.
    /** @brief Opens an update after the target slot and generation are fixed. */
    void openUpdate(bool replacement);

    std::array<std::uint8_t, kCapacityBytes> stagingImage_{};
    FlashCommitGate* commitGate_ = nullptr;
    std::uint32_t stagedGeneration_ = 0U;
    std::uint8_t stagedTargetSlot_ = 0U;
    CommitOutcome lastOutcome_ = CommitOutcome::None;
    bool updateOpen_ = false;
    bool updateDirty_ = false;
    bool replacement_ = false;
};

}  // namespace clockfw::hal
