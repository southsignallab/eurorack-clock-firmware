/**
 * @file external_input_capture.h
 * @brief Interrupt-driven capture boundary for conditioned SYNC and RST comparator signals.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace clockfw::hal {

/** @brief One comparator transition with a wrapping microsecond timestamp. */
struct ExternalInputEdge {
    std::uint32_t timestampUs = 0U;
    bool high = false;
    bool continuityLost = false;
};

/**
 * @brief Captures external comparator transitions without involving the foreground loop.
 *
 * The current prototype backend uses GPIO CHANGE interrupts. The public edge queue is
 * deliberately timer-agnostic so the timer-capable PA8 route can replace SYNC timestamp production
 * with STM32 timer input capture without changing the real-time synchronization service.
 */
class ExternalInputCapture final {
public:
    /** @brief Configures assigned comparator GPIOs and attaches edge interrupts. */
    void begin();

    /** @brief Pops the oldest captured SYNC edge; false means the queue is empty. */
    bool popSyncEdge(ExternalInputEdge& edge);

    /** @brief Pops the oldest captured RST edge; false means the queue is empty. */
    bool popResetEdge(ExternalInputEdge& edge);

    /** @brief Returns the most recently observed SYNC comparator level. */
    bool syncLevelHigh() const;

    /** @brief Returns the most recently observed RST comparator level. */
    bool resetLevelHigh() const;

    /** @brief Returns the number of SYNC edges dropped because the queue was saturated. */
    std::uint32_t droppedSyncEdges() const;

    /** @brief Returns the number of RST edges collapsed because the queue was saturated. */
    std::uint32_t collapsedResetEdges() const;

    /** @brief Monotonic sequence incremented for every observed SYNC or RST transition. */
    std::uint32_t activitySequence() const;

#ifdef CLOCK_HOST_TEST
    /** @brief Injects a captured SYNC transition for deterministic host tests. */
    void injectSyncEdgeForTest(std::uint32_t timestampUs, bool high);

    /** @brief Injects a captured RST transition for deterministic host tests. */
    void injectResetEdgeForTest(std::uint32_t timestampUs, bool high);

    /** @brief Opposite edges queued for merged interrupts, both inputs (host tests). */
    std::uint32_t synthesizedEdgesForTest() const { return synthesizedEdgesForTest_; }
#endif

private:
    static constexpr std::uint8_t kQueueCapacity = 16U;

    /**
     * @brief Single-producer/single-consumer ring of captured edges (F-025).
     *
     * Producer: EXTI handler, priority 4. Consumer: TIM3 scheduler, priority 0, which
     * preempts the producer at any instruction. The producer writes the slot and
     * then publishes head with release; the consumer loads head with acquire before
     * it reads the slot, and frees the slot by storing tail with release. Without
     * this ordering the compiler may move the slot store behind the head store, and
     * the consumer would read a stale slot. std::atomic states the order for compiler
     * and CPU at once (PM0214 Rev 10, 2.2.4: where the order of memory accesses
     * matters, software must include a barrier); on the Cortex-M4 it adds one DMB per
     * ordered access.
     */
    struct EdgeQueue {
        std::array<ExternalInputEdge, kQueueCapacity> edges{};
        std::atomic<std::uint8_t> head{0U};
        std::atomic<std::uint8_t> tail{0U};
        /** Published with release after the overflow payload below. */
        std::atomic<bool> overflowPending{false};
        volatile std::uint32_t overflowTimestampUs = 0U;
        volatile bool overflowLevelHigh = false;
        volatile std::uint32_t overflowCount = 0U;
    };
    static_assert(std::atomic<std::uint8_t>::is_always_lock_free, "queue indices must not need a lock");
    static_assert(std::atomic<bool>::is_always_lock_free, "overflow flag must not need a lock");

    /** @brief Shared EXTI thunk for the SYNC comparator GPIO. */
    static void syncInterruptThunk();

    /** @brief Shared EXTI thunk for the RST comparator GPIO. */
    static void resetInterruptThunk();

    /** @brief Captures the current SYNC GPIO level and timestamp from interrupt context. */
    void captureSyncFromIsr();

    /** @brief Captures the current RST GPIO level and timestamp from interrupt context. */
    void captureResetFromIsr();

    /**
     * @brief Queues the edges behind one EXTI interrupt (parity rule, F-016, D-15).
     * @param queue Edge queue of the input.
     * @param storedLevelHigh Level latch of the input; set to @p high.
     * @param high Level read after the pending bit was cleared.
     *
     * A changed level is one edge. An unchanged level means at least two edges
     * merged into one interrupt: the missing opposite edge is queued first, then
     * the observed edge, both with the same timestamp.
     */
    void captureFromIsr(EdgeQueue& queue, volatile bool& storedLevelHigh, bool high);

    /** @brief Pushes one edge into a single-producer/single-consumer ISR queue. */
    static void pushFromIsr(EdgeQueue& queue, ExternalInputEdge edge);

    /** @brief Pops one edge, including a continuity-loss marker for the first dropped edge. */
    static bool pop(EdgeQueue& queue, ExternalInputEdge& edge);

    static ExternalInputCapture* activeInstance_;
    EdgeQueue syncQueue_{};
    EdgeQueue resetQueue_{};
    volatile bool syncLevelHigh_ = false;
    volatile bool resetLevelHigh_ = false;
    volatile std::uint32_t activitySequence_ = 0U;
#ifdef CLOCK_HOST_TEST
    std::uint32_t synthesizedEdgesForTest_ = 0U;
#endif
};

}  // namespace clockfw::hal
