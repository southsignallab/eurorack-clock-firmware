/**
 * @file exti_dispatch.inc
 * @brief Included real-time cases: shared EXTI vector dispatch and settled level reads (F-024, F-016).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

namespace exti_fake {
/** Fake EXTI_PR contents; bit n = line n. */
std::uint32_t pendingMask = 0U;
/** Pending reads before the fake aborts a loop that does not end. */
std::uint32_t readsLeft = 0U;
std::uint32_t handledMasks[16]{};
std::uint32_t handledCount = 0U;
/** Levels returned by successive pin reads, and edges that set the pending bit again. */
bool levels[8]{};
std::uint32_t levelReads = 0U;
std::uint32_t repends = 0U;
std::uint32_t clears = 0U;

void reset(const std::uint32_t mask) {
    pendingMask = mask;
    readsLeft = 64U;
    handledCount = 0U;
    levelReads = 0U;
    repends = 0U;
    clears = 0U;
}
bool isPending(const std::uint32_t lineMask) {
    if (readsLeft == 0U) {
        throw std::runtime_error("EXTI dispatch loop does not end");
    }
    --readsLeft;
    return (pendingMask & lineMask) != 0U;
}
void handle(const std::uint32_t lineMask) {
    handledMasks[handledCount % 16U] = lineMask;
    ++handledCount;
}
bool readLevel(const std::uint32_t) {
    const bool level = levels[levelReads % 8U];
    ++levelReads;
    // Each modelled edge after a read sets the pending bit again.
    if (repends != 0U) {
        --repends;
        pendingMask |= 1U << 8U;
    }
    return level;
}
void clear(const std::uint32_t lineMask) {
    pendingMask &= ~lineMask;
    ++clears;
}
}  // namespace exti_fake

/** @brief Runs one dispatch and reports whether it ended. */
bool dispatchEnds(const std::uint32_t first, const std::uint32_t last) {
    try {
        hal::exti::dispatchPendingLines(first, last, exti_fake::isPending, exti_fake::handle);
        return true;
    } catch (const std::runtime_error&) {
        return false;
    }
}

void testExti15To10DispatchEndsAndServesLine15() {
    // F-024: a 16-bit mask shifted past bit 15 became 0 and the loop never ended.
    exti_fake::reset(1U << 15U);
    CHECK(dispatchEnds(hal::exti::kExti15To10FirstLine, hal::exti::kExti15To10LastLine));
    CHECK_EQ(exti_fake::handledCount, 1U);
    CHECK_EQ(exti_fake::handledMasks[0], 1U << 15U);

    exti_fake::reset((1U << 10U) | (1U << 12U) | (1U << 15U) | (1U << 9U));
    CHECK(dispatchEnds(hal::exti::kExti15To10FirstLine, hal::exti::kExti15To10LastLine));
    CHECK_EQ(exti_fake::handledCount, 3U);
    CHECK_EQ(exti_fake::handledMasks[0], 1U << 10U);
    CHECK_EQ(exti_fake::handledMasks[1], 1U << 12U);
    CHECK_EQ(exti_fake::handledMasks[2], 1U << 15U);
    // Six lines, six pending reads.
    CHECK_EQ(exti_fake::readsLeft, 64U - 6U);
}

void testExti9To5DispatchServesTheInputLines() {
    // DIY board: SYNC on EXTI 8 (PA8), RST on EXTI 9 (PA9).
    exti_fake::reset((1U << 8U) | (1U << 9U) | (1U << 10U) | (1U << 4U));
    CHECK(dispatchEnds(hal::exti::kExti9To5FirstLine, hal::exti::kExti9To5LastLine));
    CHECK_EQ(exti_fake::handledCount, 2U);
    CHECK_EQ(exti_fake::handledMasks[0], 1U << 8U);
    CHECK_EQ(exti_fake::handledMasks[1], 1U << 9U);
    CHECK_EQ(exti_fake::readsLeft, 64U - 5U);

    exti_fake::reset(0U);
    CHECK(dispatchEnds(hal::exti::kExti9To5FirstLine, hal::exti::kExti9To5LastLine));
    CHECK_EQ(exti_fake::handledCount, 0U);
}

void testSettledLevelReadsAgainAfterAnEdgeInTheWindow() {
    constexpr std::uint32_t kLine = 1U << 8U;
    // No edge after the clear: one read, pending stays clear.
    exti_fake::reset(0U);
    exti_fake::levels[0] = true;
    CHECK(hal::exti::readSettledLevel(0xA08U, kLine, exti_fake::readLevel, exti_fake::isPending, exti_fake::clear));
    CHECK_EQ(exti_fake::levelReads, 1U);
    CHECK_EQ(exti_fake::clears, 0U);

    // The fall of a short pulse lands between clear and read: the first read already
    // sees LOW, the bit is set again; it is cleared and read again, nothing is left
    // pending for a second interrupt.
    exti_fake::reset(0U);
    exti_fake::levels[0] = false;
    exti_fake::levels[1] = false;
    exti_fake::repends = 1U;
    CHECK(!hal::exti::readSettledLevel(0xA08U, kLine, exti_fake::readLevel, exti_fake::isPending, exti_fake::clear));
    CHECK_EQ(exti_fake::levelReads, 2U);
    CHECK_EQ(exti_fake::clears, 1U);
    CHECK_EQ(exti_fake::pendingMask & kLine, 0U);

    // A level that keeps toggling ends after kMaximumLevelRereads and leaves the bit set.
    exti_fake::reset(0U);
    for (std::uint32_t index = 0U; index < 8U; ++index) {
        exti_fake::levels[index] = (index & 1U) != 0U;
    }
    exti_fake::repends = 100U;
    CHECK(hal::exti::readSettledLevel(0xA08U, kLine, exti_fake::readLevel, exti_fake::isPending, exti_fake::clear));
    CHECK_EQ(exti_fake::levelReads, 1U + hal::exti::kMaximumLevelRereads);
    CHECK_EQ(exti_fake::clears, hal::exti::kMaximumLevelRereads);
    CHECK((exti_fake::pendingMask & kLine) != 0U);
}
