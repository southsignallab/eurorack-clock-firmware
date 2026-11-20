/**
 * @file crc32.h
 * @brief Table-driven CRC-32 (IEEE 802.3, reflected) shared by every persistent record format.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace clockfw::hal {

/**
 * @brief Computes CRC-32 with polynomial 0xEDB88320, initial value 0xFFFFFFFF and
 * final XOR 0xFFFFFFFF.
 *
 * The result is bit-identical to the historical bitwise implementation used by
 * every stored record, so existing Flash images stay valid. The 256-entry table
 * is a compile-time constant in Flash; it replaces eight shift/XOR steps per
 * byte with one lookup (F-009).
 *
 * @param data First byte of the input; read only, not retained.
 * @param size Number of bytes to read from @p data.
 * @return CRC-32 of the @p size bytes.
 */
std::uint32_t crc32(const std::uint8_t* data, std::size_t size);

}  // namespace clockfw::hal
