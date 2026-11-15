/**
 * @file timebase.inc
 * @brief Milliseconds and microseconds derive from one 64-bit TIM5 time base (F-046, F-064).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * The fake counter stands in for TIM5. The scheduler interrupt advances the
 * extension every 50 us; a Flash operation stalls the CPU, so neither that
 * interrupt nor SysTick runs while TIM5 keeps counting.
 */

namespace timebase_test {
std::uint32_t counterUs = 0U;
hal::ExtendedTimebase* interruptingTimebase = nullptr;
std::uint32_t interruptsDuringRead = 0U;

std::uint32_t readCounter() { return counterUs; }

/** Simulates the scheduler interrupt hitting between the anchor and the counter read. */
std::uint32_t readCounterWithInterrupt() {
    if (interruptsDuringRead != 0U && interruptingTimebase != nullptr) {
        --interruptsDuringRead;
        counterUs += config::kSchedulerTickUs;
        interruptingTimebase->advanceFromIsr(counterUs);
    }
    return counterUs;
}

/** Runs the scheduler interrupt for @p durationUs; @p sysTickMs counts like SysTick. */
void runInterrupts(
    hal::ExtendedTimebase& timebase,
    const std::uint32_t durationUs,
    std::uint32_t& sysTickMs) {
    constexpr std::uint32_t kTickUs = config::kSchedulerTickUs;
    for (std::uint32_t elapsedUs = 0U; elapsedUs < durationUs; elapsedUs += kTickUs) {
        counterUs += config::kSchedulerTickUs;
        timebase.advanceFromIsr(counterUs);
        if (counterUs % 1000U == 0U) {
            ++sysTickMs;
        }
    }
}
}  // namespace timebase_test

void testMillisecondsDoNotDriftFromMicrosecondsAcrossFlashStalls() {
    using namespace timebase_test;
    hal::ExtendedTimebase timebase;
    counterUs = 0U;
    std::uint32_t sysTickMs = 0U;
    runInterrupts(timebase, 1'000'000U, sysTickMs);
    for (std::uint32_t stall = 0U; stall < 3U; ++stall) {
        // Sector erase: CPU stalled for 1.5 s, TIM5 counts, no interrupt runs.
        counterUs += 1'500'000U;
        runInterrupts(timebase, 200'000U, sysTickMs);
    }
    const std::uint64_t nowUs = timebase.read(readCounter);
    CHECK_EQ(nowUs, static_cast<std::uint64_t>(counterUs));
    CHECK_EQ(hal::ExtendedTimebase::millisecondsFrom(nowUs), counterUs / 1000U);
    // The former SysTick source lost the stalled 4.5 s (F-046).
    CHECK_EQ(counterUs / 1000U - sysTickMs, 4500U);
}

void testExtendedTimebaseCrossesTheTim5Wrap() {
    using namespace timebase_test;
    hal::ExtendedTimebase timebase;
    counterUs = 0U;
    std::uint32_t sysTickMs = 0U;
    // Run to 100 us before the 32-bit wrap (71.6 min), then 1 ms past it.
    counterUs = 0xFFFFFFFFU - 99U - config::kSchedulerTickUs;
    timebase.advanceFromIsr(counterUs);
    const std::uint64_t beforeUs = timebase.read(readCounter);
    runInterrupts(timebase, 1100U, sysTickMs);
    const std::uint64_t afterUs = timebase.read(readCounter);
    CHECK(counterUs < 2000U);
    CHECK_EQ(afterUs - beforeUs, 1100ULL);
    CHECK_EQ(afterUs, (1ULL << 32U) + counterUs);
    CHECK_EQ(static_cast<std::uint32_t>(afterUs), counterUs);
    // Milliseconds continue across the wrap instead of restarting at zero.
    const std::uint32_t afterMs = hal::ExtendedTimebase::millisecondsFrom(afterUs);
    CHECK(afterMs > hal::ExtendedTimebase::millisecondsFrom(beforeUs));
    CHECK_EQ(afterMs, static_cast<std::uint32_t>(afterUs / 1000ULL));
}

void testExtendedTimebaseReadsDuringTheWrapAndAnInterrupt() {
    using namespace timebase_test;
    hal::ExtendedTimebase timebase;
    // Anchor 16 us before the wrap, read 16 us after it without an interrupt in between.
    counterUs = 0xFFFFFFF0U;
    timebase.advanceFromIsr(counterUs);
    counterUs = 0x00000010U;
    CHECK_EQ(timebase.read(readCounter), (1ULL << 32U) + 0x10U);
    // An interrupt advancing the anchor between the anchor and the counter read
    // makes the reader retry; the result is the counter value of the retry.
    interruptingTimebase = &timebase;
    interruptsDuringRead = 1U;
    const std::uint64_t retriedUs = timebase.read(readCounterWithInterrupt);
    CHECK_EQ(retriedUs, (1ULL << 32U) + 0x10U + config::kSchedulerTickUs);
    CHECK_EQ(interruptsDuringRead, 0U);
    // An interrupt that crosses the wrap during the read is folded in as well.
    hal::ExtendedTimebase crossing;
    counterUs = 0xFFFFFFE0U;
    crossing.advanceFromIsr(counterUs);
    interruptingTimebase = &crossing;
    interruptsDuringRead = 1U;
    CHECK_EQ(crossing.read(readCounterWithInterrupt), (1ULL << 32U) + 0x12U);
    interruptingTimebase = nullptr;
}

void testMillisecondsStayOnTheMicrosecondBaseOverHours() {
    using namespace timebase_test;
    hal::ExtendedTimebase timebase;
    counterUs = 0U;
    std::uint64_t trueUs = 0U;
    // Three hours (two TIM5 wraps) in 997 us steps with an interrupt each step.
    for (std::uint32_t step = 0U; step < 10'833'500U; ++step) {
        counterUs += 997U;
        trueUs += 997U;
        timebase.advanceFromIsr(counterUs);
        if (step % 4099U == 0U) {
            const std::uint64_t nowUs = timebase.read(readCounter);
            CHECK_EQ(nowUs, trueUs);
            const std::uint64_t nowMs = hal::ExtendedTimebase::millisecondsFrom(nowUs);
            CHECK(nowMs * 1000ULL <= nowUs && nowUs < nowMs * 1000ULL + 1000ULL);
        }
    }
    CHECK(trueUs > 2ULL * (1ULL << 32U));
}

void testTapAtMicrosecondZeroAfterTheWrapKeepsTheMicrosecondBase() {
    // F-064: TIM5 reads exactly 0 once per wrap. That pass must not fall back to
    // milliseconds x 1000, which is a different time base.
    resetFakes(); prepareDisplaySuccess();
    hal::OledDisplay display; CHECK(display.begin());
    ClockState state = makeDefaultState();
    hal::GateOutputDriver gates; gates.beginDisabled();
    engine::ClockEngine engine(gates); engine.begin(state);
    hal::PersistentStorage::resetForTest(); hal::PersistentStorage storage;
    services::PersistentStateService persistence(storage); persistence.begin();
    ui::UiRenderer renderer(display, persistence);
    ui::UiController controller(state, engine, renderer, persistence);

    constexpr std::uint32_t kNowMs = 5000000U;  // x 1000 is far from every microsecond value below
    const auto tapAtUs = [&](const std::uint32_t releasedUs) {
        hal::ControlSample sample{};
        sample.tapButton = pressedEdge();
        controller.processControls(sample, kNowMs, releasedUs - 1000U);
        sample = {};
        sample.tapButton = releasedEdge();
        controller.processControls(sample, kNowMs, releasedUs);
    };
    CHECK(state.bpm != 150U);
    tapAtUs(0U - 400000U);
    tapAtUs(0U);
    CHECK_EQ(state.bpm, 150U);
    tapAtUs(400000U);
    CHECK_EQ(state.bpm, 150U);
    // The second and third tap continue one sequence.
    CHECK_EQ(controller.navigation().tapIndicatorFrame, 1U);
}
