/**
 * @file exti_dispatch.h
 * @brief Pending-line dispatch and settled level reads for shared EXTI interrupt vectors.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * Register access stays in platform_io.cpp; these functions take it as callbacks so
 * the host tests can drive them with a fake pending mask (F-024, F-016).
 */
#pragma once

#include <cstdint>

namespace clockfw::hal::exti {

/** @brief Returns whether the EXTI_PR bit of @p lineMask is set. */
using PendingReader = bool (*)(std::uint32_t lineMask);

/** @brief Services one pending line given as its EXTI_PR bit mask. */
using LineHandler = void (*)(std::uint32_t lineMask);

/** @brief Clears the EXTI_PR bit of @p lineMask (write 1, RM0368 10.3.6). */
using PendingClearer = void (*)(std::uint32_t lineMask);

/** @brief Returns the input level of one GPIO pin (true = HIGH). */
using LevelReader = bool (*)(std::uint32_t pin);

/** @brief First line of the shared EXTI9_5 vector. */
inline constexpr std::uint32_t kExti9To5FirstLine = 5U;
/** @brief Last line of the shared EXTI9_5 vector. */
inline constexpr std::uint32_t kExti9To5LastLine = 9U;
/** @brief First line of the shared EXTI15_10 vector. */
inline constexpr std::uint32_t kExti15To10FirstLine = 10U;
/** @brief Last line of the shared EXTI15_10 vector. */
inline constexpr std::uint32_t kExti15To10LastLine = 15U;

/** @brief Bound for readSettledLevel(): a level that keeps changing ends the loop anyway. */
inline constexpr std::uint32_t kMaximumLevelRereads = 3U;

/**
 * @brief Calls @p handle for every line in [@p firstLine, @p lastLine] that @p isPending reports.
 *
 * Iterates over bit indices, so the last line 15 ends the loop (F-024).
 * Requires @p lastLine <= 15.
 *
 * @param firstLine First EXTI line number to check.
 * @param lastLine Last EXTI line number to check, inclusive, at most 15.
 * @param isPending Reads the EXTI_PR bit of one line.
 * @param handle Called with the EXTI_PR bit mask of every pending line, in ascending order.
 *
 * @isr Runs in the EXTI input-edge interrupts: bounded (at most 16 lines), non-blocking, no
 * allocation, no output, opens no interrupt lock; @p handle runs in the same context.
 */
void dispatchPendingLines(
    std::uint32_t firstLine,
    std::uint32_t lastLine,
    PendingReader isPending,
    LineHandler handle);

/**
 * @brief Reads the level of @p pin after its EXTI line was cleared, so that level and
 *        pending bit agree.
 * @param pin GPIO pin behind the line.
 * @param lineMask EXTI_PR bit of the line.
 * @param readLevel Reads @p pin.
 * @param isPending Reads the EXTI_PR bit.
 * @param clearPending Clears the EXTI_PR bit.
 * @return The level read after the last clear.
 *
 * An edge between the clear and the read already shows in the level but sets the
 * pending bit again; the next interrupt would then see the same level although no
 * further edge came and count a merged pulse that did not happen. While the bit is
 * set again after a read, it is cleared and the level read again, at most
 * kMaximumLevelRereads times.
 *
 * @isr Runs in the EXTI input-edge interrupts: bounded (at most kMaximumLevelRereads
 * rereads), non-blocking, no allocation, no output, opens no interrupt lock.
 */
bool readSettledLevel(
    std::uint32_t pin,
    std::uint32_t lineMask,
    LevelReader readLevel,
    PendingReader isPending,
    PendingClearer clearPending);

}  // namespace clockfw::hal::exti
