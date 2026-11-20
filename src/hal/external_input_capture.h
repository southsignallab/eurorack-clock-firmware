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
    /** SystemClock::microseconds() when the interrupt captured the edge; wraps at 2^32 us. */
    std::uint32_t timestampUs = 0U;
    /** Comparator level after the edge: true = HIGH. */
    bool high = false;
    /**
     * True for the marker of the first edge dropped on a full queue: edges between this
     * and the previous popped edge are missing.
     */
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

    /**
     * @brief Pops the oldest captured SYNC edge; false means the queue is empty.
     * @param edge Receives the edge; unchanged when the call returns false.
     * @return True when @p edge holds a queued edge or the continuity-loss marker.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    bool popSyncEdge(ExternalInputEdge& edge);

    /**
     * @brief Pops the oldest captured RST edge; false means the queue is empty.
     * @param edge Receives the edge; unchanged when the call returns false.
     * @return True when @p edge holds a queued edge or the continuity-loss marker.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    bool popResetEdge(ExternalInputEdge& edge);

    /**
     * @brief Returns the most recently observed SYNC comparator level.
     * @return True for HIGH, as latched by begin() or the last SYNC interrupt.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    bool syncLevelHigh() const;

    /**
     * @brief Returns the most recently observed RST comparator level.
     * @return True for HIGH, as latched by begin() or the last RST interrupt.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    bool resetLevelHigh() const;

    /**
     * @brief Returns the number of SYNC edges dropped because the queue was saturated.
     * @return Dropped SYNC edges since start, wrapping at 2^32.
     */
    std::uint32_t droppedSyncEdges() const;

    /**
     * @brief Returns the number of RST edges collapsed because the queue was saturated.
     * @return Dropped RST edges since start, wrapping at 2^32.
     */
    std::uint32_t collapsedResetEdges() const;

#ifdef CLOCK_HOST_TEST
    /** @brief Injects a captured SYNC transition for deterministic host tests. */
    void injectSyncEdgeForTest(std::uint32_t timestampUs, bool high);

    /** @brief Injects a captured RST transition for deterministic host tests. */
    void injectResetEdgeForTest(std::uint32_t timestampUs, bool high);

    /** @brief Opposite edges queued for merged interrupts, both inputs (host tests). */
    std::uint32_t synthesizedEdgesForTest() const {
        return synthesizedEdgesForTest_;
    }
#endif

  private:
    /** Ring slots per input; one slot stays free, so a queue holds at most 15 edges. */
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
        /** Ring storage; slot head is written by the producer, slot tail read by the consumer. */
        std::array<ExternalInputEdge, kQueueCapacity> edges{};
        /** Next slot to write; written only by the producer (EXTI). */
        std::atomic<std::uint8_t> head{0U};
        /** Next slot to read; written only by the consumer (TIM3). */
        std::atomic<std::uint8_t> tail{0U};
        /** Published with release after the overflow payload below. */
        std::atomic<bool> overflowPending{false};
        /** Timestamp in microseconds of the first dropped edge. */
        volatile std::uint32_t overflowTimestampUs = 0U;
        /** Level of the first dropped edge: true = HIGH. */
        volatile bool overflowLevelHigh = false;
        /** Edges dropped since start; written only by the producer, wraps at 2^32. */
        volatile std::uint32_t overflowCount = 0U;
    };
    static_assert(
        std::atomic<std::uint8_t>::is_always_lock_free,
        "queue indices must not need a lock");
    static_assert(std::atomic<bool>::is_always_lock_free, "overflow flag must not need a lock");

    /**
     * @brief Shared EXTI thunk for the SYNC comparator GPIO.
     *
     * Forwards to captureSyncFromIsr() of the instance set by begin(); does nothing before.
     *
     * @isr Runs in the EXTI input-edge interrupts: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    static void syncInterruptThunk();

    /**
     * @brief Shared EXTI thunk for the RST comparator GPIO.
     *
     * Forwards to captureResetFromIsr() of the instance set by begin(); does nothing before.
     *
     * @isr Runs in the EXTI input-edge interrupts: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    static void resetInterruptThunk();

    /**
     * @brief Captures the current SYNC GPIO level and timestamp from interrupt context.
     *
     * @isr Runs in the EXTI input-edge interrupts: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    void captureSyncFromIsr();

    /**
     * @brief Captures the current RST GPIO level and timestamp from interrupt context.
     *
     * @isr Runs in the EXTI input-edge interrupts: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
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
     *
     * @isr Runs in the EXTI input-edge interrupts: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    void captureFromIsr(EdgeQueue& queue, volatile bool& storedLevelHigh, bool high);

    /**
     * @brief Pushes one edge into a single-producer/single-consumer ISR queue.
     * @param queue Queue of the input; only the producer side is touched.
     * @param edge Edge to queue.
     *
     * On a full queue the edge is dropped, counted, and, if no loss marker is pending,
     * kept as the loss marker. While a marker is pending, every new edge is dropped and
     * counted until the consumer has taken the marker.
     *
     * @isr Runs in the EXTI input-edge interrupts: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    static void pushFromIsr(EdgeQueue& queue, ExternalInputEdge edge);

    /**
     * @brief Pops one edge, including a continuity-loss marker for the first dropped edge.
     * @param queue Queue of the input; only the consumer side is written.
     * @param edge Receives the oldest edge, or the marker with continuityLost set once the
     * queue is empty; unchanged when the call returns false.
     * @return True when @p edge was written.
     *
     * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
     * opens no interrupt lock.
     */
    static bool pop(EdgeQueue& queue, ExternalInputEdge& edge);

    /** Instance that the static interrupt thunks forward to; set by begin(). */
    static ExternalInputCapture* activeInstance_;
    /** SYNC edges from the EXTI producer to the TIM3 consumer. */
    EdgeQueue syncQueue_{};
    /** RST edges from the EXTI producer to the TIM3 consumer. */
    EdgeQueue resetQueue_{};
    /** Last SYNC level, written by begin() and the SYNC interrupt. */
    volatile bool syncLevelHigh_ = false;
    /** Last RST level, written by begin() and the RST interrupt. */
    volatile bool resetLevelHigh_ = false;
#ifdef CLOCK_HOST_TEST
    std::uint32_t synthesizedEdgesForTest_ = 0U;
#endif
};

}  // namespace clockfw::hal
