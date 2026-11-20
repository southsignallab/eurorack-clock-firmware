/**
 * @file platform_io.h
 * @brief Framework-independent GPIO, interrupt, time, and MCU initialization boundary.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once
#include "hal/gpio_pin.h"
#include <cstdint>

namespace clockfw::hal::platform {
/** @brief Function called from an EXTI interrupt for one registered pin. */
using InterruptCallback = void (*)();
/** @brief Edge selection for attachInterrupt(): Rising, Falling, or Change (both edges). */
enum class InterruptEdge : std::uint8_t { Rising, Falling, Change };
/** @brief Initializes HAL, the 84 MHz system clock, and the 1 MHz TIM5 timebase. */
void initializeMcu();
/**
 * @brief Configures one GPIO as an input with its internal pull-up enabled.
 * @param pin GPIO to configure; a pin on an unsupported port is ignored.
 */
void configureInputPullup(mcu::Pin pin);
/**
 * @brief Configures one GPIO as a push-pull output.
 * @param pin GPIO to configure; its output level is not set here.
 */
void configureOutput(mcu::Pin pin);
/**
 * @brief Sets the output level of one GPIO, then makes it a push-pull output.
 * @param pin GPIO to configure.
 * @param high Level the pin drives from the moment it becomes an output.
 *
 * The output data bit is written while the pin is still an input, so the pin never drives
 * another level, not even for one bus cycle (V3).
 */
void configureOutputAtLevel(mcu::Pin pin, bool high);
/**
 * @brief Reads one GPIO logic level.
 * @param pin GPIO to read.
 * @return True for HIGH; false for LOW or a pin on an unsupported port.
 *
 * @isr Runs in the EXTI input-edge interrupts: bounded, non-blocking, no allocation, no output,
 * opens no interrupt lock.
 */
bool read(mcu::Pin pin);
/**
 * @brief Reads the level of an EXTI input inside its interrupt callback (F-016).
 *
 * Production clears the pending bit again and rereads while an edge set it after
 * the read (exti::readSettledLevel()), so the next interrupt does not count an edge
 * that this level already contains. Host and simulator read the pin.
 *
 * @param pin EXTI input pin whose line is being serviced.
 * @return True for HIGH, read after the last clear of the pending bit.
 *
 * @isr Runs in the EXTI input-edge interrupts: bounded (at most
 * exti::kMaximumLevelRereads rereads), non-blocking, no allocation, no output, opens no
 * interrupt lock.
 */
bool readInterruptLevel(mcu::Pin pin);
/**
 * @brief Writes one GPIO logic level.
 * @param pin GPIO to drive; a pin on an unsupported port is ignored.
 * @param high True drives HIGH, false LOW.
 */
void write(mcu::Pin pin, bool high);
/**
 * @brief Sets and clears lines of one GPIO port in a single BSRR register write.
 * @param portReference Any pin of the target port; only its port is used.
 * @param setMask Lines driven HIGH (bit n = line n).
 * @param resetMask Lines driven LOW; must not overlap @p setMask.
 *
 * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
 * opens no interrupt lock.
 */
void writePortLines(mcu::Pin portReference, std::uint16_t setMask, std::uint16_t resetMask);
#if defined(CLOCK_HOST_TEST)
/** @brief One recorded writePortLines() call in deterministic host tests. */
struct PortWriteRecord {
    std::uint32_t port = 0U;
    std::uint16_t setMask = 0U;
    std::uint16_t resetMask = 0U;
};
/** @brief Returns how many writePortLines() calls happened since the last reset. */
std::uint32_t portWriteCountForTest();
/** @brief Returns the most recent writePortLines() call. */
PortWriteRecord lastPortWriteForTest();
/** @brief Clears the writePortLines() call log. */
void resetPortWriteLogForTest();
#endif
/**
 * @brief Registers an EXTI callback for one encoded GPIO pin.
 * @param pin Input pin, configured as an EXTI input with pull-up. Callbacks are kept per
 * line number, so a later pin with the same line number replaces the callback.
 * @param callback Called from the EXTI interrupt of the pin's line.
 * @param edge Edges that trigger the interrupt.
 *
 * Production sets the EXTI interrupt preemption priority to 4 and enables the vector.
 */
void attachInterrupt(mcu::Pin pin, InterruptCallback callback, InterruptEdge edge);
/**
 * @brief Starts the dedicated quadrature-encoder backend for the supplied A/B pins.
 *
 * Production uses TIM4 encoder mode on PB6/PB7 so contact transitions continue to
 * be counted even while foreground code or interrupts are temporarily delayed.
 * Host/simulator builds provide an equivalent software counter for deterministic tests.
 *
 * @param phaseA Encoder phase A pin; production accepts only PB6.
 * @param phaseB Encoder phase B pin; production accepts only PB7.
 * @return true when the encoder backend was configured successfully.
 */
bool beginQuadratureEncoder(mcu::Pin phaseA, mcu::Pin phaseB);
/**
 * @brief Returns the wrapping signed-position counter in raw quadrature transitions.
 * @return Count that wraps at 2^32. Production extends the 16-bit TIM4 count by its signed
 * 16-bit change since the previous call.
 */
std::uint32_t quadratureEncoderCount();
/**
 * @brief Returns the current A/B electrical phase as bit1=A and bit0=B.
 * @return 0..3; a bit is 1 when its input reads HIGH.
 */
std::uint8_t quadratureEncoderState();
/**
 * @brief Returns milliseconds elapsed since MCU initialization.
 *
 * Production derives them from the 64-bit extension of the TIM5 microsecond counter,
 * so they share one time base with microseconds() and keep counting through Flash
 * operations that stall SysTick (F-046). They wrap at 2^32 ms.
 *
 * @return Milliseconds since MCU initialization, wrapping at 2^32 ms.
 */
std::uint32_t milliseconds();
/**
 * @brief Returns the wrapping 32-bit TIM5 microsecond counter (low word of the same time
 * base).
 * @return TIM5 counter value in microseconds, wrapping at 2^32 us.
 *
 * @isr Runs in the TIM3 scheduler interrupt and the EXTI input-edge interrupts: bounded,
 * non-blocking, no allocation, no output, opens no interrupt lock.
 */
std::uint32_t microseconds();
/**
 * @brief Scheduler-ISR only: folds the TIM5 counter into the 64-bit time base (single
 * writer).
 *
 * @isr Runs in the TIM3 scheduler interrupt: bounded, non-blocking, no allocation, no output,
 * opens no interrupt lock.
 */
void advanceTimebaseFromIsr();
/**
 * @brief Blocks foreground execution for the requested milliseconds.
 * @param durationMs Delay in milliseconds; production calls HAL_Delay().
 */
void delayMilliseconds(std::uint32_t durationMs);
/**
 * @brief Disables interrupts and returns the previous PRIMASK state.
 * @return PRIMASK before the call; 0 means interrupts were enabled.
 */
std::uint32_t enterCritical();
/**
 * @brief Restores the PRIMASK state captured by enterCritical().
 * @param previousPrimask Value returned by the matching enterCritical(); interrupts are
 * enabled again only when it is 0.
 */
void exitCritical(std::uint32_t previousPrimask);
}  // namespace clockfw::hal::platform
