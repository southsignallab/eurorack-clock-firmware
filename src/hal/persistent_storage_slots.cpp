/**
 * @file persistent_storage_slots.cpp
 * @brief A/B slot validation, cached scan, and physical Flash commit for STM32F401 and host tests.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/persistent_storage_slots.h"

#include <algorithm>
#include <cstring>

#include "hal/crc32.h"
#ifndef CLOCK_HOST_TEST
#include <stm32f4xx_hal.h>
#endif

namespace clockfw::hal::persistent_slots {
namespace {
static_assert(
    kSlotHeaderBytes + PersistentStorage::kMaximumImageBytes <= PersistentStorage::kPhysicalSlotBytes,
    "12-KiB persistent-image policy must fit inside one 16-KiB slot");
static_assert((PersistentStorage::kCapacityBytes % 4U) == 0U, "Flash image must be word-aligned");

bool generationIsNewer(const std::uint32_t candidate, const std::uint32_t reference) {
    return static_cast<std::int32_t>(candidate - reference) > 0;
}

bool supportedPayloadSize(const std::uint32_t size) {
    return size == PersistentStorage::kCapacityBytes || size == kV1PayloadBytes ||
        size == kLegacyPayloadBytes;
}

// Every physical slot change goes through commitImage() (or a host-test seed),
// which invalidates first. Flash content cannot change otherwise while the
// firmware runs, so one validation per boot is sufficient (F-009).
SlotScan gScan{};
bool gScanValid = false;
std::uint32_t gContentRevision = 0U;

#ifdef CLOCK_HOST_TEST
std::array<std::array<std::uint8_t, PersistentStorage::kPhysicalSlotBytes>, kSlotCount> gHostSlots = [] {
    std::array<std::array<std::uint8_t, PersistentStorage::kPhysicalSlotBytes>, kSlotCount> slots{};
    for (auto& slot : slots) {
        slot.fill(kErasedByte);
    }
    return slots;
}();
HostFaults gHostFaults{};
#else
constexpr std::uintptr_t kSlotAddresses[kSlotCount] = {0x08004000UL, 0x08008000UL};
constexpr std::uint32_t kSlotSectors[kSlotCount] = {FLASH_SECTOR_1, FLASH_SECTOR_2};

bool eraseSlotUnlocked(const std::size_t slotIndex) {
    FLASH_EraseInitTypeDef erase{};
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = kSlotSectors[slotIndex];
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    std::uint32_t sectorError = 0U;
    return HAL_FLASHEx_Erase(&erase, &sectorError) == HAL_OK;
}

bool programWordUnlocked(const std::uintptr_t address, const std::uint32_t value) {
    return HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_WORD,
        static_cast<std::uint32_t>(address),
        static_cast<std::uint64_t>(value)) == HAL_OK;
}

bool programWordsUnlocked(
    const std::uintptr_t address,
    const std::uint8_t* const bytes,
    const std::size_t size) {
    if ((size % sizeof(std::uint32_t)) != 0U) {
        return false;
    }
    for (std::size_t offset = 0U; offset < size; offset += sizeof(std::uint32_t)) {
        std::uint32_t word = 0U;
        std::memcpy(&word, bytes + offset, sizeof(word));
        if (!programWordUnlocked(address + offset, word)) {
            return false;
        }
    }
    return true;
}
#endif

SlotKind classify(const std::size_t slotIndex, SlotHeader& header) {
    std::memcpy(&header, slotBase(slotIndex), sizeof(header));
    if (header.magic != kSlotMagic || header.commitMarker != kCommitMarker ||
        header.headerCrc32 != headerCrc32(header)) {
        return SlotKind::Empty;
    }
    if (header.formatVersion != kSlotFormatVersion || !supportedPayloadSize(header.payloadSize)) {
        // An intact, committed header in an unknown format was written by newer
        // firmware. Treating it as empty would let the first autosave overwrite it.
        return SlotKind::Foreign;
    }
#ifdef CLOCK_HOST_TEST
    ++gHostFaults.payloadValidations;
#endif
    const std::uint32_t payloadCrc =
        crc32(slotBase(slotIndex) + kPayloadOffset, static_cast<std::size_t>(header.payloadSize));
    return payloadCrc == header.payloadCrc32 ? SlotKind::Valid : SlotKind::Empty;
}

int newestOf(const SlotScan& result, const bool includeForeign) {
    int newest = -1;
    for (std::size_t index = 0U; index < kSlotCount; ++index) {
        const SlotKind kind = result.kinds[index];
        if (kind == SlotKind::Empty || (kind == SlotKind::Foreign && !includeForeign)) {
            continue;
        }
        if (newest < 0 || generationIsNewer(
                result.headers[index].generation,
                result.headers[static_cast<std::size_t>(newest)].generation)) {
            newest = static_cast<int>(index);
        }
    }
    return newest;
}
}  // namespace

std::uint32_t headerCrc32(const SlotHeader& header) {
    // The first five words are immutable metadata. headerCrc/reserved/commit are excluded.
    return crc32(reinterpret_cast<const std::uint8_t*>(&header), 5U * sizeof(std::uint32_t));
}

SlotHeader sealedHeader(
    const std::uint32_t generation,
    const std::uint8_t* const payload,
    const std::size_t payloadSize) {
    SlotHeader header{};
    header.generation = generation;
    header.payloadSize = static_cast<std::uint32_t>(payloadSize);
    header.payloadCrc32 = crc32(payload, payloadSize);
    header.headerCrc32 = headerCrc32(header);
    header.commitMarker = kCommitMarker;
    return header;
}

const std::uint8_t* slotBase(const std::size_t slotIndex) {
#ifdef CLOCK_HOST_TEST
    return gHostSlots[slotIndex].data();
#else
    return reinterpret_cast<const std::uint8_t*>(kSlotAddresses[slotIndex]);
#endif
}

const SlotScan& scan() {
    if (!gScanValid) {
        SlotScan result{};
        for (std::size_t index = 0U; index < kSlotCount; ++index) {
            result.kinds[index] = classify(index, result.headers[index]);
        }
        result.active = newestOf(result, false);
        result.newestCommitted = newestOf(result, true);
        gScan = result;
        gScanValid = true;
    }
    return gScan;
}

void invalidateScan() {
    gScanValid = false;
    ++gContentRevision;
}

std::uint32_t contentRevision() {
    return gContentRevision;
}

bool commitImage(
    const std::size_t targetSlot,
    const std::uint32_t generation,
    const std::array<std::uint8_t, PersistentStorage::kCapacityBytes>& image) {
    const SlotHeader header = sealedHeader(generation, image.data(), image.size());
    const SlotScan previous = scan();
    invalidateScan();

#ifdef CLOCK_HOST_TEST
    auto& slot = gHostSlots[targetSlot];
    slot.fill(kErasedByte);
    std::memcpy(slot.data(), &header, kCommitWordIndex * sizeof(std::uint32_t));
    std::copy(image.begin(), image.end(), slot.begin() + static_cast<std::ptrdiff_t>(kPayloadOffset));
    if (gHostFaults.powerLossBeforeCommit) {
        gHostFaults.powerLossBeforeCommit = false;
        return false;
    }
    std::memcpy(
        slot.data() + static_cast<std::ptrdiff_t>(kCommitWordIndex * sizeof(std::uint32_t)),
        &kCommitMarker,
        sizeof(kCommitMarker));
#else
    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }
    // A stale error flag would make FLASH_WaitForLastOperation() fail the next erase.
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
#ifdef FLASH_FLAG_RDERR
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_RDERR);
#endif
    const std::uintptr_t base = kSlotAddresses[targetSlot];
    const bool programmed =
        eraseSlotUnlocked(targetSlot) &&
        programWordsUnlocked(
            base,
            reinterpret_cast<const std::uint8_t*>(&header),
            kCommitWordIndex * sizeof(std::uint32_t)) &&
        programWordsUnlocked(base + kPayloadOffset, image.data(), image.size()) &&
        // COMMIT is deliberately the last programmed word. Until this succeeds,
        // boot validation must continue to prefer the previous generation.
        programWordUnlocked(base + kCommitWordIndex * sizeof(std::uint32_t), kCommitMarker);
    (void)HAL_FLASH_Lock();
    if (!programmed) {
        return false;
    }
#endif

    // Verification re-reads only the target slot; the untouched slot keeps its
    // classification. The result becomes the new cache.
    SlotScan verified = previous;
    verified.kinds[targetSlot] = classify(targetSlot, verified.headers[targetSlot]);
    verified.active = newestOf(verified, false);
    verified.newestCommitted = newestOf(verified, true);
    gScan = verified;
    gScanValid = true;
    return verified.active == static_cast<int>(targetSlot) &&
        verified.headers[targetSlot].generation == generation;
}

#ifdef CLOCK_HOST_TEST
HostFaults& hostFaults() {
    return gHostFaults;
}

std::uint8_t* mutableSlotBase(const std::size_t slotIndex) {
    return gHostSlots[slotIndex].data();
}

void eraseHostSlots() {
    for (auto& slot : gHostSlots) {
        slot.fill(kErasedByte);
    }
    invalidateScan();
}
#endif

}  // namespace clockfw::hal::persistent_slots
