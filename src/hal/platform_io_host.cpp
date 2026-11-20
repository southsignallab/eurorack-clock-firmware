/**
 * @file platform_io_host.cpp
 * @brief Deterministic host-test and simulator adapter of the platform I/O boundary.
 *
 * The STM32CubeF4 production implementation is platform_io.cpp.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "hal/platform_io.h"

#if defined(CLOCK_HOST_TEST) || defined(CLOCK_SIMULATOR)
#include <Arduino.h>
#include <cassert>

namespace clockfw::hal::platform {
namespace {
std::uint32_t gQuadraturePhaseA = mcu::kUnassigned;
std::uint32_t gQuadraturePhaseB = mcu::kUnassigned;
std::uint8_t gQuadraturePreviousState = 0U;
std::uint32_t gQuadratureCount = 0U;
constexpr std::int8_t kQuadratureTransitions[16] =
    {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
void quadratureEdgeThunk() {
    const std::uint8_t currentState = (digitalRead(gQuadraturePhaseA) == HIGH ? 2U : 0U) |
        (digitalRead(gQuadraturePhaseB) == HIGH ? 1U : 0U);
    const std::uint8_t transitionIndex = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(gQuadraturePreviousState) << 2U) | currentState);
    gQuadraturePreviousState = currentState;
    gQuadratureCount += static_cast<std::uint32_t>(
        static_cast<std::int32_t>(kQuadratureTransitions[transitionIndex]));
}
}  // namespace

void initializeMcu() {}
void configureInputPullup(const mcu::Pin pin) {
    pinMode(pin, INPUT_PULLUP);
}
void configureOutput(const mcu::Pin pin) {
    pinMode(pin, OUTPUT);
}
void configureOutputAtLevel(const mcu::Pin pin, const bool high) {
    digitalWrite(pin, high ? HIGH : LOW);
    pinMode(pin, OUTPUT);
}
bool read(const mcu::Pin pin) {
    return digitalRead(pin) == HIGH;
}
bool readInterruptLevel(const mcu::Pin pin) {
    return read(pin);
}
void write(const mcu::Pin pin, const bool high) {
    digitalWrite(pin, high ? HIGH : LOW);
}
#if defined(CLOCK_HOST_TEST)
namespace {
std::uint32_t gPortWriteCount = 0U;
PortWriteRecord gLastPortWrite{};
}  // namespace
std::uint32_t portWriteCountForTest() {
    return gPortWriteCount;
}
PortWriteRecord lastPortWriteForTest() {
    return gLastPortWrite;
}
void resetPortWriteLogForTest() {
    gPortWriteCount = 0U;
    gLastPortWrite = PortWriteRecord{};
}
#endif
void writePortLines(
    const mcu::Pin portReference,
    const std::uint16_t setMask,
    const std::uint16_t resetMask) {
    // Production writes one BSRR word; a line in both masks would be ambiguous.
    assert((setMask & resetMask) == 0U);
    const std::uint32_t portBase = portReference & 0xFF00U;
#if defined(CLOCK_HOST_TEST)
    ++gPortWriteCount;
    gLastPortWrite = PortWriteRecord{portBase >> 8U, setMask, resetMask};
#endif
    for (std::uint32_t line = 0U; line < 16U; ++line) {
        const std::uint32_t lineMask = 1U << line;
        if ((resetMask & lineMask) != 0U) {
            digitalWrite(portBase | line, LOW);
        } else if ((setMask & lineMask) != 0U) {
            digitalWrite(portBase | line, HIGH);
        }
    }
}
void attachInterrupt(
    const mcu::Pin pin,
    const InterruptCallback callback,
    const InterruptEdge edge) {
    const int mode = edge == InterruptEdge::Rising
        ? RISING
        : (edge == InterruptEdge::Falling ? FALLING : CHANGE);
    ::attachInterrupt(digitalPinToInterrupt(pin), callback, mode);
}
bool beginQuadratureEncoder(const mcu::Pin phaseA, const mcu::Pin phaseB) {
    gQuadraturePhaseA = phaseA;
    gQuadraturePhaseB = phaseB;
    configureInputPullup(phaseA);
    configureInputPullup(phaseB);
    gQuadraturePreviousState =
        (digitalRead(phaseA) == HIGH ? 2U : 0U) | (digitalRead(phaseB) == HIGH ? 1U : 0U);
    gQuadratureCount = 0U;
    ::attachInterrupt(digitalPinToInterrupt(phaseA), quadratureEdgeThunk, CHANGE);
    ::attachInterrupt(digitalPinToInterrupt(phaseB), quadratureEdgeThunk, CHANGE);
    return true;
}
std::uint32_t quadratureEncoderCount() {
    return gQuadratureCount;
}
#if defined(CLOCK_HOST_TEST)
void setQuadratureEncoderCountForTest(const std::uint32_t count) {
    gQuadratureCount = count;
}
#endif
std::uint8_t quadratureEncoderState() {
    return static_cast<std::uint8_t>(
        (digitalRead(gQuadraturePhaseA) == HIGH ? 2U : 0U) |
        (digitalRead(gQuadraturePhaseB) == HIGH ? 1U : 0U));
}
std::uint32_t milliseconds() {
    return millis();
}
std::uint32_t microseconds() {
    return micros();
}
void advanceTimebaseFromIsr() {}
void delayMilliseconds(const std::uint32_t durationMs) {
    delay(durationMs);
}
std::uint32_t enterCritical() {
    noInterrupts();
    return 0U;
}
void exitCritical(const std::uint32_t) {
    interrupts();
}
}  // namespace clockfw::hal::platform
#endif
