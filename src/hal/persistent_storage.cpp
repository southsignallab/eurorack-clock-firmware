/**
 * @file persistent_storage.cpp
 * @brief Power-loss-safe A/B Flash storage API for STM32F401 and host tests.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/persistent_storage.h"

#include <algorithm>
#include <cstring>

#include "hal/persistent_storage_slots.h"

namespace clockfw::hal {
namespace slots = persistent_slots;

bool PersistentStorage::readBytes(
    const std::size_t offset,
    std::uint8_t* const destination,
    const std::size_t size) const {
    if (destination == nullptr || !isRangeValid(offset, size)) {
        return false;
    }
#ifdef CLOCK_HOST_TEST
    ++slots::hostFaults().reads;
    if (slots::hostFaults().failNextRead) {
        slots::hostFaults().failNextRead = false;
        return false;
    }
#endif
    std::fill_n(destination, size, slots::kErasedByte);
    const slots::SlotScan& current = slots::scan();
    if (current.active < 0) {
        return true;
    }
    const std::size_t activeSlot = static_cast<std::size_t>(current.active);
    const std::size_t payloadSize = static_cast<std::size_t>(current.headers[activeSlot].payloadSize);
    if (offset < payloadSize) {
        const std::size_t readable = std::min<std::size_t>(size, payloadSize - offset);
        std::memcpy(destination, slots::slotBase(activeSlot) + slots::kPayloadOffset + offset, readable);
    }
    return true;
}

std::uint32_t PersistentStorage::contentRevision() {
    return slots::contentRevision();
}

bool PersistentStorage::writeLocked() const {
    return slots::scan().writeLocked();
}

void PersistentStorage::openUpdate(const bool replacement) {
    const slots::SlotScan& current = slots::scan();
    // The next generation follows the newest committed slot of any format and
    // goes to the other slot. Without a foreign slot this is the classic A/B
    // alternation against the active slot.
    if (current.newestCommitted < 0) {
        stagedTargetSlot_ = 0U;
        stagedGeneration_ = 1U;
    } else {
        const std::size_t newest = static_cast<std::size_t>(current.newestCommitted);
        stagedTargetSlot_ = newest == 0U ? 1U : 0U;
        stagedGeneration_ = current.headers[newest].generation + 1U;
    }
    stagingImage_.fill(slots::kErasedByte);
    if (!replacement && current.active >= 0) {
        const std::size_t activeSlot = static_cast<std::size_t>(current.active);
        const std::size_t previousSize = std::min<std::size_t>(
            stagingImage_.size(), static_cast<std::size_t>(current.headers[activeSlot].payloadSize));
        std::memcpy(stagingImage_.data(), slots::slotBase(activeSlot) + slots::kPayloadOffset, previousSize);
    }
    updateOpen_ = true;
    updateDirty_ = replacement;
    replacement_ = replacement;
}

bool PersistentStorage::beginUpdate() {
#ifdef CLOCK_HOST_TEST
    if (slots::hostFaults().failNextRead) {
        slots::hostFaults().failNextRead = false;
        return false;
    }
#endif
    if (slots::scan().writeLocked()) {
        lastOutcome_ = CommitOutcome::Locked;
        return false;
    }
    openUpdate(false);
    return true;
}

bool PersistentStorage::beginFullReplacement() {
    openUpdate(true);
    return true;
}

bool PersistentStorage::stageBytes(
    const std::size_t offset,
    const std::uint8_t* const source,
    const std::size_t size) {
    if (!updateOpen_ || source == nullptr || !isRangeValid(offset, size)) {
        return false;
    }
    auto destination = stagingImage_.begin() + static_cast<std::ptrdiff_t>(offset);
    if (!std::equal(source, source + size, destination)) {
        std::copy_n(source, size, destination);
        updateDirty_ = true;
    }
    return true;
}

bool PersistentStorage::stageFill(
    const std::size_t offset,
    const std::size_t size,
    const std::uint8_t value) {
    if (!updateOpen_ || !isRangeValid(offset, size)) {
        return false;
    }
    auto first = stagingImage_.begin() + static_cast<std::ptrdiff_t>(offset);
    auto last = first + static_cast<std::ptrdiff_t>(size);
    if (!std::all_of(first, last, [value](const std::uint8_t byte) { return byte == value; })) {
        std::fill(first, last, value);
        updateDirty_ = true;
    }
    return true;
}

bool PersistentStorage::commitUpdate() {
    if (!updateOpen_) {
        return false;
    }
    if (!updateDirty_) {
        cancelUpdate();
        lastOutcome_ = CommitOutcome::Unchanged;
        return true;
    }
    if (!replacement_ && slots::scan().writeLocked()) {
        cancelUpdate();
        lastOutcome_ = CommitOutcome::Locked;
        return false;
    }
    // The gate is consulted after all staging work and immediately before the
    // erase, so the window between the engine check and Flash access is minimal.
    if (commitGate_ != nullptr && !commitGate_->tryOpenCommitWindow()) {
        cancelUpdate();
        lastOutcome_ = CommitOutcome::Deferred;
        return false;
    }
#ifdef CLOCK_HOST_TEST
    ++slots::hostFaults().commitAttempts;
    const bool injectedFailure = slots::hostFaults().failNextWrite || slots::hostFaults().failAllWrites;
    slots::hostFaults().failNextWrite = false;
    const bool committed =
        !injectedFailure && slots::commitImage(stagedTargetSlot_, stagedGeneration_, stagingImage_);
#else
    const bool committed = slots::commitImage(stagedTargetSlot_, stagedGeneration_, stagingImage_);
#endif
    if (commitGate_ != nullptr) {
        commitGate_->closeCommitWindow();
    }
    cancelUpdate();
    lastOutcome_ = committed ? CommitOutcome::Committed : CommitOutcome::Failed;
#ifdef CLOCK_HOST_TEST
    if (committed) {
        ++slots::hostFaults().commitCount;
    }
#endif
    return committed;
}

void PersistentStorage::cancelUpdate() {
    updateOpen_ = false;
    updateDirty_ = false;
    replacement_ = false;
}

bool PersistentStorage::writeBytes(
    const std::size_t offset,
    const std::uint8_t* const source,
    const std::size_t size) {
    if (source == nullptr || !isRangeValid(offset, size)) {
        return false;
    }
    if (!beginUpdate()) {
        return false;
    }
    if (!stageBytes(offset, source, size)) {
        cancelUpdate();
        return false;
    }
    return commitUpdate();
}

#ifdef CLOCK_HOST_TEST
namespace {
bool seedHostSlot(
    const std::size_t slotIndex,
    const slots::SlotHeader& header,
    const std::uint8_t* const payload,
    const std::size_t payloadSize) {
    std::uint8_t* const slot = slots::mutableSlotBase(slotIndex);
    std::fill_n(slot, PersistentStorage::kPhysicalSlotBytes, slots::kErasedByte);
    std::memcpy(slot, &header, sizeof(header));
    if (payload != nullptr) {
        std::memcpy(slot + slots::kPayloadOffset, payload, payloadSize);
    }
    slots::invalidateScan();
    return true;
}
}  // namespace

void PersistentStorage::resetForTest() {
    slots::eraseHostSlots();
    slots::hostFaults() = slots::HostFaults{};
}
void PersistentStorage::simulateRebootForTest() {
    slots::invalidateScan();
    slots::hostFaults().payloadValidations = 0U;
    slots::hostFaults().reads = 0U;
}
std::uint32_t PersistentStorage::writeCommitCountForTest() {
    return slots::hostFaults().commitCount;
}
std::uint32_t PersistentStorage::payloadValidationCountForTest() {
    return slots::hostFaults().payloadValidations;
}
std::uint32_t PersistentStorage::readCountForTest() {
    return slots::hostFaults().reads;
}
void PersistentStorage::failNextReadForTest() {
    slots::hostFaults().failNextRead = true;
}
void PersistentStorage::failNextWriteForTest() {
    slots::hostFaults().failNextWrite = true;
}
void PersistentStorage::failAllWritesForTest(const bool fail) {
    slots::hostFaults().failAllWrites = fail;
}
std::uint32_t PersistentStorage::commitAttemptCountForTest() {
    return slots::hostFaults().commitAttempts;
}
void PersistentStorage::powerLossBeforeCommitForTest() {
    slots::hostFaults().powerLossBeforeCommit = true;
}
void PersistentStorage::corruptNewestPayloadByteForTest(const std::size_t offset) {
    const int activeSlot = slots::scan().active;
    if (offset >= kCapacityBytes || activeSlot < 0) {
        return;
    }
    slots::mutableSlotBase(static_cast<std::size_t>(activeSlot))[slots::kPayloadOffset + offset] ^= 0x01U;
    slots::invalidateScan();
}
void PersistentStorage::corruptNewestHeaderWordForTest(
    const std::size_t wordIndex,
    const std::uint32_t value) {
    const int activeSlot = slots::scan().active;
    if (wordIndex >= slots::kHeaderWordCount || activeSlot < 0) {
        return;
    }
    std::memcpy(
        slots::mutableSlotBase(static_cast<std::size_t>(activeSlot)) + wordIndex * sizeof(std::uint32_t),
        &value,
        sizeof(value));
    slots::invalidateScan();
}
bool PersistentStorage::seedLegacyImageForTest(const std::uint8_t* const image, const std::size_t size) {
    if (image == nullptr || size != slots::kLegacyPayloadBytes) {
        return false;
    }
    resetForTest();
    (void)seedHostSlot(0U, slots::sealedHeader(7U, image, size), image, size);
    return slots::scan().active == 0 && slots::scan().headers[0].payloadSize == size;
}
bool PersistentStorage::seedV1ImageForTest(const std::uint8_t* const image, const std::size_t size) {
    if (image == nullptr || size != slots::kV1PayloadBytes) {
        return false;
    }
    resetForTest();
    (void)seedHostSlot(0U, slots::sealedHeader(8U, image, size), image, size);
    return slots::scan().active == 0 && slots::scan().headers[0].payloadSize == size;
}
void PersistentStorage::seedForeignSlotForTest(
    const std::size_t slotIndex,
    const std::uint32_t formatVersion,
    const std::uint32_t generation,
    const bool intactHeaderCrc,
    const std::uint32_t payloadSize) {
    if (slotIndex >= slots::kSlotCount) {
        return;
    }
    slots::SlotHeader header{};
    header.formatVersion = formatVersion;
    header.generation = generation;
    header.payloadSize = payloadSize;
    header.payloadCrc32 = 0x12345678UL;
    header.headerCrc32 = slots::headerCrc32(header) ^ (intactHeaderCrc ? 0U : 1U);
    header.commitMarker = slots::kCommitMarker;
    (void)seedHostSlot(slotIndex, header, nullptr, 0U);
}
#endif

bool PersistentStorage::isRangeValid(const std::size_t offset, const std::size_t size) {
    return offset <= kCapacityBytes && size <= kCapacityBytes - offset;
}
}  // namespace clockfw::hal
