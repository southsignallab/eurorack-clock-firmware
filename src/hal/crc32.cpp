/**
 * @file crc32.cpp
 * @brief Compile-time CRC-32 table and lookup loop.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/crc32.h"

#include <array>

namespace clockfw::hal {
namespace {
constexpr std::uint32_t kPolynomial = 0xEDB88320UL;
constexpr std::uint32_t kInitialValue = 0xFFFFFFFFUL;

constexpr std::array<std::uint32_t, 256U> makeTable() {
    std::array<std::uint32_t, 256U> table{};
    for (std::uint32_t index = 0U; index < 256U; ++index) {
        std::uint32_t value = index;
        for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
            value = (value & 1U) != 0U ? (value >> 1U) ^ kPolynomial : value >> 1U;
        }
        table[index] = value;
    }
    return table;
}

constexpr std::array<std::uint32_t, 256U> kTable = makeTable();
static_assert(kTable[1] == 0x77073096UL, "CRC-32 table must use the reflected IEEE polynomial");
}  // namespace

std::uint32_t crc32(const std::uint8_t* const data, const std::size_t size) {
    std::uint32_t crc = kInitialValue;
    for (std::size_t index = 0U; index < size; ++index) {
        crc = (crc >> 8U) ^ kTable[(crc ^ data[index]) & 0xFFU];
    }
    return crc ^ kInitialValue;
}

}  // namespace clockfw::hal
