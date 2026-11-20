/**
 * @file text_format.h
 * @brief Bounded printf-style formatting with one documented truncation rule (D-16 point 3).
 *
 * Truncation rule: the text is cut at the buffer end and the buffer always holds a NUL-terminated
 * prefix of the full text. A cut is allowed for display text; formatInto() reports it with false so
 * that a caller can react, and host tests count every cut (truncationCountForTest()).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace clockfw::text {

/**
 * @brief Formats like std::snprintf() into destination, which always ends NUL-terminated.
 * @param destination Output buffer; may be null only when size is 0.
 * @param size Capacity of destination in bytes, including the terminating NUL.
 * @param format printf format string; checked against the arguments by the compiler.
 * @return true when the complete text fit; false when it was truncated or the format failed
 *         (destination then holds the fitting prefix, or the empty string after a format error).
 */
// NOLINTNEXTLINE(cert-dcl50-cpp): C-style variadic for compiler format checking (see .cpp)
[[gnu::format(printf, 3, 4)]] bool formatInto(
    char* destination,
    std::size_t size,
    const char* format,
    ...);

#if defined(CLOCK_HOST_TEST)
/** @brief Number of truncated or failed formatInto() calls since the last reset (host tests only). */
std::uint32_t truncationCountForTest();

/** @brief Resets truncationCountForTest() to zero (host tests only). */
void resetTruncationCountForTest();
#endif

}  // namespace clockfw::text
