/**
 * @file simulator_persistence.h
 * @brief File-backed persistence adapter for the native clock simulator.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>

#include "hal/persistent_storage.h"

namespace clockfw::sim {

/** @brief Mirrors the host-test persistence bytes to a normal desktop file. */
class SimulatorPersistence final {
public:
    /** @brief Constructs the adapter for one simulator-state file. */
    explicit SimulatorPersistence(std::filesystem::path path);

    /** @brief Loads an existing state file into the firmware's simulated NVM before boot. */
    void load();

    /** @brief Writes current simulated NVM to disk when contents changed. */
    void flushIfChanged();

    /** @brief Forces the current simulated NVM image to disk, unless file writes are disabled. */
    void flush();

    /**
     * @brief Enables or disables every file write of this adapter (default: enabled).
     *
     * A host that runs the firmware on a real-time thread, such as VCV Rack's audio thread,
     * disables writes here and mirrors exported images from its own worker instead. While
     * disabled, the simulated NVM is authoritative: after the first load() or importImage(),
     * later power-ups keep it instead of re-reading a possibly older file.
     */
    void setFileWritesEnabled(bool enabled);

    /** @brief Returns path used for durable simulator state. */
    const std::filesystem::path& path() const;

    /** @brief Exports the complete logical persistence image for host containers such as VCV Rack. */
    std::array<std::uint8_t, hal::PersistentStorage::kCapacityBytes> exportImage() const;

    /** @brief Replaces the complete logical persistence image and its durable host mirror. */
    void importImage(const std::array<std::uint8_t, hal::PersistentStorage::kCapacityBytes>& image);

    /**
     * @brief Writes one image to @p path through a temporary file and rename.
     * @throws std::runtime_error when the file cannot be written or replaced.
     */
    static void writeImageFile(
        const std::filesystem::path& path,
        const std::array<std::uint8_t, hal::PersistentStorage::kCapacityBytes>& image);

    /** @brief Number of image files written by the calling thread, for host tests. */
    static std::uint64_t fileWritesOnThisThreadForTest();

private:
    /** @brief Reads the complete host-storage image through the real persistence HAL API. */
    std::array<std::uint8_t, hal::PersistentStorage::kCapacityBytes> readImage() const;

    std::filesystem::path path_;
    std::array<std::uint8_t, hal::PersistentStorage::kCapacityBytes> lastFlushedImage_{};
    bool hasLastImage_ = false;
    bool fileWritesEnabled_ = true;
};

}  // namespace clockfw::sim
