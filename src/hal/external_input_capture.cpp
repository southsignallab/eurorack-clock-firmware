/**
 * @file external_input_capture.cpp
 * @brief Interrupt-driven capture boundary for conditioned SYNC and RST comparator signals.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/external_input_capture.h"

#include "hal/platform_io.h"

#include "hal/system_clock.h"
#include "pin_map.h"

#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
#include "hal/runtime_instrumentation.h"
#endif

namespace clockfw::hal {

ExternalInputCapture* ExternalInputCapture::activeInstance_ = nullptr;

void ExternalInputCapture::begin() {
    activeInstance_ = this;
    if (pinmap::kExternalSyncSignalPin != pinmap::kUnassignedDigitalPin) {
        platform::configureInputPullup(pinmap::kExternalSyncSignalPin);
        syncLevelHigh_ = platform::read(pinmap::kExternalSyncSignalPin);
        platform::attachInterrupt(pinmap::kExternalSyncSignalPin, syncInterruptThunk, platform::InterruptEdge::Change);
    }
    if (pinmap::kExternalResetSignalPin != pinmap::kUnassignedDigitalPin) {
        platform::configureInputPullup(pinmap::kExternalResetSignalPin);
        resetLevelHigh_ = platform::read(pinmap::kExternalResetSignalPin);
        platform::attachInterrupt(pinmap::kExternalResetSignalPin, resetInterruptThunk, platform::InterruptEdge::Change);
    }
}

bool ExternalInputCapture::popSyncEdge(ExternalInputEdge& edge) {
    return pop(syncQueue_, edge);
}

bool ExternalInputCapture::popResetEdge(ExternalInputEdge& edge) {
    return pop(resetQueue_, edge);
}

bool ExternalInputCapture::syncLevelHigh() const {
    return syncLevelHigh_;
}

bool ExternalInputCapture::resetLevelHigh() const {
    return resetLevelHigh_;
}

std::uint32_t ExternalInputCapture::droppedSyncEdges() const {
    return syncQueue_.overflowCount;
}

std::uint32_t ExternalInputCapture::collapsedResetEdges() const {
    return resetQueue_.overflowCount;
}

void ExternalInputCapture::syncInterruptThunk() {
    if (activeInstance_ != nullptr) {
        activeInstance_->captureSyncFromIsr();
    }
}

void ExternalInputCapture::resetInterruptThunk() {
    if (activeInstance_ != nullptr) {
        activeInstance_->captureResetFromIsr();
    }
}

void ExternalInputCapture::captureSyncFromIsr() {
    captureFromIsr(syncQueue_, syncLevelHigh_, platform::readInterruptLevel(pinmap::kExternalSyncSignalPin));
}

void ExternalInputCapture::captureResetFromIsr() {
    captureFromIsr(resetQueue_, resetLevelHigh_, platform::readInterruptLevel(pinmap::kExternalResetSignalPin));
}

void ExternalInputCapture::captureFromIsr(
    EdgeQueue& queue,
    volatile bool& storedLevelHigh,
    const bool high) {
    const std::uint32_t timestampUs = SystemClock::microseconds();
    // The pending bit was set, so at least one edge came. The same level as last
    // time means an even number, at least two: a pulse shorter than the latency.
    const bool merged = high == storedLevelHigh;
    storedLevelHigh = high;
    if (merged) {
#ifdef CLOCK_HOST_TEST
        ++synthesizedEdgesForTest_;
#endif
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
        instrumentation::countSynthesizedInputEdge();
#endif
        pushFromIsr(queue, {timestampUs, !high});
    }
    pushFromIsr(queue, {timestampUs, high});
}

void ExternalInputCapture::pushFromIsr(EdgeQueue& queue, const ExternalInputEdge edge) {
    // Once continuity has been lost, do not enqueue newer edges behind the
    // unpublished loss boundary. Otherwise the consumer could observe newer
    // timestamps first and the older continuity marker afterwards. Drop until
    // TIM3 has drained the queue and consumed the marker; the next physical edge
    // then starts a clean acquisition epoch. RST gate mode still has the separate
    // resetLevelHigh_ level latch, so its authoritative level is not lost here.
    if (queue.overflowPending.load(std::memory_order_acquire)) {
        ++queue.overflowCount;
        return;
    }

    // Only this producer writes head; only the consumer writes tail.
    const std::uint8_t head = queue.head.load(std::memory_order_relaxed);
    const std::uint8_t next = static_cast<std::uint8_t>((head + 1U) % kQueueCapacity);
    if (next == queue.tail.load(std::memory_order_acquire)) {
        ++queue.overflowCount;
        // TIM3 intentionally has higher priority than EXTI and may preempt this
        // producer. Publish the overflow marker only after its multi-field payload
        // is complete. Retaining the first dropped edge is enough to declare the
        // exact boundary after which the queued transition stream is no longer
        // continuous.
        queue.overflowTimestampUs = edge.timestampUs;
        queue.overflowLevelHigh = edge.high;
        queue.overflowPending.store(true, std::memory_order_release);
        return;
    }
    queue.edges[head] = edge;
    // Release: the slot above is complete before the consumer can see it.
    queue.head.store(next, std::memory_order_release);
}

bool ExternalInputCapture::pop(EdgeQueue& queue, ExternalInputEdge& edge) {
    // Acquire on head: the slot is read after the producer's release of head.
    const std::uint8_t tail = queue.tail.load(std::memory_order_relaxed);
    if (tail != queue.head.load(std::memory_order_acquire)) {
        edge = queue.edges[tail];
        // Release: the slot is read before the producer may reuse it.
        queue.tail.store(
            static_cast<std::uint8_t>((tail + 1U) % kQueueCapacity),
            std::memory_order_release);
        return true;
    }
    if (!queue.overflowPending.load(std::memory_order_acquire)) {
        return false;
    }
    edge.timestampUs = queue.overflowTimestampUs;
    edge.high = queue.overflowLevelHigh;
    edge.continuityLost = true;
    queue.overflowPending.store(false, std::memory_order_release);
    return true;
}

#ifdef CLOCK_HOST_TEST
void ExternalInputCapture::injectSyncEdgeForTest(
    const std::uint32_t timestampUs,
    const bool high) {
    syncLevelHigh_ = high;
    pushFromIsr(syncQueue_, {timestampUs, high});
}

void ExternalInputCapture::injectResetEdgeForTest(
    const std::uint32_t timestampUs,
    const bool high) {
    resetLevelHigh_ = high;
    pushFromIsr(resetQueue_, {timestampUs, high});
}
#endif

}  // namespace clockfw::hal
