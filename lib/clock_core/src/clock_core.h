/**
 * @file clock_core.h
 * @brief Hardware-independent timing, rhythm, and pattern mathematics.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

#include "domain/clock_types.h"

namespace clockfw::core {

/** Q32 fixed-point representation of exactly one musical beat. */
inline constexpr std::uint64_t kQ32One = 1ULL << 32U;

/**
 * @brief Calculates the greatest common divisor of two unsigned integers.
 * @param first First value.
 * @param second Second value.
 * @return The greatest common divisor, or 1 when both inputs are zero.
 *
 * @isr TIM3 scheduler interrupt: bounded Euclidean loop (logarithmic in the inputs),
 *      non-blocking, no allocation, opens no interrupt lock.
 */
std::uint32_t greatestCommonDivisor(std::uint32_t first, std::uint32_t second);

/**
 * @brief Resolves one channel's integer and rational rate into a reduced fraction.
 * @param settings Shared channel settings containing divide/multiply and ratio values.
 * @param numerator Receives the reduced rate numerator.
 * @param denominator Receives the reduced rate denominator.
 *
 * @isr TIM3 scheduler interrupt: bounded (one GCD reduction), non-blocking, no allocation,
 *      opens no interrupt lock.
 */
void calculateEffectiveRate(
    const CommonChannelSettings& settings,
    std::uint32_t& numerator,
    std::uint32_t& denominator);

/**
 * @brief Resolves CLOCK mode rate including local/master meter unit relationship.
 * @param settings Shared channel settings.
 * @param localBeatUnit Local CLOCK channel beat unit.
 * @param masterBeatUnit Master beat unit.
 * @param numerator Receives the reduced rate numerator.
 * @param denominator Receives the reduced rate denominator.
 *
 * @isr TIM3 scheduler interrupt: bounded (two GCD reductions), non-blocking, no allocation,
 *      opens no interrupt lock.
 */
void calculateEffectiveClockRate(
    const CommonChannelSettings& settings,
    std::uint8_t localBeatUnit,
    std::uint8_t masterBeatUnit,
    std::uint32_t& numerator,
    std::uint32_t& denominator);

/**
 * @brief Resolves EUCLID/SEQ rate against a sixteenth-note x1 pattern grid.
 * @param settings Shared channel rate settings.
 * @param masterBeatUnit Master meter unit used by the Q32 timeline.
 * @param numerator Receives the reduced rate numerator.
 * @param denominator Receives the reduced rate denominator.
 *
 * At x1 a pattern step is one sixteenth note. Therefore a 16-step pattern
 * spans one 4/4 bar at the master tempo. DIV/MULT and rational ratios are
 * applied on top of that musical grid.
 *
 * @isr TIM3 scheduler interrupt: bounded (two GCD reductions), non-blocking, no allocation,
 *      opens no interrupt lock.
 */
void calculateEffectivePatternRate(
    const CommonChannelSettings& settings,
    std::uint8_t masterBeatUnit,
    std::uint32_t& numerator,
    std::uint32_t& denominator);

/**
 * @brief Returns the next Q32 channel interval while preserving division remainder.
 * @param numerator Rate numerator; zero is treated as one.
 * @param denominator Rate denominator; zero is treated as one.
 * @param remainder Persistent integer remainder carried into the next interval.
 * @return Next interval in Q32 beats, never zero.
 *
 * @isr TIM3 scheduler interrupt: constant time (one 64-bit division), non-blocking, no
 *      allocation, opens no interrupt lock.
 */
std::uint64_t calculateNextIntervalQ32(
    std::uint32_t numerator,
    std::uint32_t denominator,
    volatile std::uint32_t& remainder);

/**
 * @brief Returns one scheduler-tick increment of the master Q32 timeline.
 * @param bpm Master tempo in beats per minute.
 * @param beatUnit Master meter unit.
 * @param schedulerFrequencyHz Scheduler interrupt frequency.
 * @param remainder Persistent division remainder preventing long-term drift.
 * @return Q32 master-position increment for one scheduler tick.
 */
std::uint64_t calculateMasterIncrementQ32(
    std::uint16_t bpm,
    std::uint8_t beatUnit,
    std::uint32_t schedulerFrequencyHz,
    volatile std::uint64_t& remainder);

/**
 * @brief Returns one scheduler-tick increment from a milli-BPM tempo.
 * @param bpmMilli Master tempo in thousandths of a BPM.
 * @param beatUnit Master meter unit.
 * @param schedulerFrequencyHz Scheduler interrupt frequency.
 * @param remainder Persistent division remainder preventing long-term drift.
 * @return Q32 master-position increment for one scheduler tick.
 *
 * This variant is used by the external-sync path so the filtered input tempo
 * can retain sub-BPM precision without introducing floating-point arithmetic.
 */
std::uint64_t calculateMasterIncrementMilliBpmQ32(
    std::uint32_t bpmMilli,
    std::uint8_t beatUnit,
    std::uint32_t schedulerFrequencyHz,
    volatile std::uint64_t& remainder);

/** @brief Master-position advance over one or more scheduler ticks. */
struct MasterAdvanceQ32 {
    /** Advance over all ticks in Q32 master beats. */
    std::uint64_t totalQ32 = 0U;
    /** Smallest advance of one tick (quotient without remainder carry) in Q32 master beats. */
    std::uint64_t perTickFloorQ32 = 0U;
};

/**
 * @brief Advances the master position by @p ticks scheduler ticks from a milli-BPM tempo.
 * @param bpmMilli Master tempo in thousandths of a BPM.
 * @param beatUnit Master meter unit.
 * @param schedulerFrequencyHz Scheduler interrupt frequency.
 * @param ticks Number of scheduler ticks to advance (caught-up ticks included).
 * @param remainder Persistent division remainder preventing long-term drift.
 * @return Total and one-tick advance in Q32 master beats.
 *
 * Bit-identical to @p ticks calls of calculateMasterIncrementMilliBpmQ32() but with one
 * 64-bit division regardless of @p ticks: every further tick only adds the quotient
 * and carries the remainder (F-012).
 *
 * @isr TIM3 scheduler interrupt: one 64-bit division and @p ticks loop rounds (the caller
 *      bounds ticks by config::kMaximumSchedulerCatchUpTicks), non-blocking, no allocation,
 *      opens no interrupt lock.
 */
MasterAdvanceQ32 advanceMasterPositionMilliBpmQ32(
    std::uint32_t bpmMilli,
    std::uint8_t beatUnit,
    std::uint32_t schedulerFrequencyHz,
    std::uint32_t ticks,
    volatile std::uint64_t& remainder);

/**
 * @brief Applies alternating swing to one ideal interval.
 * @param baseIntervalQ32 Ideal interval in Q32 beats.
 * @param swingPercent Swing amount in the inclusive range 0..50; larger values are clamped.
 * @param longInterval True for the elongated interval, false for the shortened interval.
 * @return Swing-adjusted non-zero interval.
 */
std::uint64_t applySwing(
    std::uint64_t baseIntervalQ32,
    std::uint8_t swingPercent,
    bool longInterval);

/**
 * @brief Converts a percentage phase setting into a Q32 interval offset.
 * @param intervalQ32 Local ideal interval.
 * @param phasePercent Phase percentage, clamped to 99.
 * @return Q32 phase offset.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
std::uint64_t calculatePhaseOffsetQ32(std::uint64_t intervalQ32, std::uint8_t phasePercent);

/**
 * @brief Advances a xorshift32 pseudo-random state by one deterministic sample.
 * @param value Current state; 0 stays 0.
 * @return Next state.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
std::uint32_t nextXorshift32(std::uint32_t value);

/**
 * @brief Applies a trigger probability without using floating-point arithmetic.
 * @param probabilityPercent Probability in percent.
 * @param randomState Mutable deterministic random state.
 * @return True when the trigger passes the probability gate.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
bool passesProbability(std::uint8_t probabilityPercent, std::uint32_t& randomState);

/**
 * @brief Converts gate length to scheduler ticks and prevents overlap with the next edge.
 * @param gateLengthMs Requested gate length in milliseconds.
 * @param schedulerTickUs Scheduler quantum in microseconds.
 * @param baseIntervalUs Ideal local interval in microseconds.
 * @param swingPercent Swing amount used to derive the shortest possible interval.
 * @return Safe gate duration in scheduler ticks, always at least one tick.
 *
 * @isr TIM3 scheduler interrupt: constant time (64-bit divisions), non-blocking, no
 *      allocation, opens no interrupt lock.
 */
std::uint32_t calculateGatePulseTicks(
    std::uint16_t gateLengthMs,
    std::uint32_t schedulerTickUs,
    std::uint64_t baseIntervalUs,
    std::uint8_t swingPercent);

/**
 * @brief Returns the next wrapped step index for a pattern of the specified length.
 * @param currentStep Zero-based current step.
 * @param patternLength Pattern length in steps; 0 is treated as 1.
 * @return (currentStep + 1) modulo the pattern length.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
std::uint8_t advanceStep(std::uint8_t currentStep, std::uint8_t patternLength);

/**
 * @brief Returns a bit mask containing exactly the requested number of low-order pattern
 *        bits.
 * @param patternLength Number of bits; 64 or more gives all bits set.
 * @return Mask with the low @p patternLength bits set; 0 for length 0.
 */
std::uint64_t patternMask(std::uint8_t patternLength);

/**
 * @brief Clears pattern bits outside the active pattern length.
 * @param pattern Gate bits, bit n = step n.
 * @param patternLength Active length in steps.
 * @return @p pattern without the bits at and above @p patternLength.
 */
std::uint64_t clampPattern(std::uint64_t pattern, std::uint8_t patternLength);

/**
 * @brief Inverts only the active portion of a binary gate pattern.
 * @param pattern Gate bits, bit n = step n.
 * @param patternLength Active length in steps.
 * @return Inverted active bits; bits outside the length are cleared.
 */
std::uint64_t invertPattern(std::uint64_t pattern, std::uint8_t patternLength);

/**
 * @brief Builds a 1010... or 0101... alternating pattern with the requested length.
 * @param patternLength Active length in steps.
 * @param firstStepOn True sets step 0 and every second step after it; false sets step 1 and
 *        every second step after it.
 * @return Gate bits, bit n = step n, limited to the length.
 */
std::uint64_t alternatingPattern(std::uint8_t patternLength, bool firstStepOn = true);

/**
 * @brief Returns whether the requested Euclidean step contains a hit.
 * @param step Zero-based step before rotation.
 * @param settings Steps, hits (capped at steps) and rotation.
 * @return True for a hit; false when steps or hits is 0.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
bool isEuclideanHit(std::uint8_t step, const EuclidSettings& settings);

/**
 * @brief Returns whether the requested sequencer step contains an active gate.
 * @param step Zero-based step before rotation.
 * @param settings Length, rotation and gate bits of the binary sequencer.
 * @return True when the rotated step's bit is set; false for length 0.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
bool isSequencerHit(std::uint8_t step, const SequencerSettings& settings);

/**
 * @brief Returns whether a channel should be reset for the requested global-reset operation.
 * @param resetMode GLOBAL or FREE policy of the channel.
 * @param includeFreeRunningChannels True to reset FREE channels as well.
 * @return True for GLOBAL channels, and for every channel when FREE ones are included.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
bool shouldResetChannel(ResetMode resetMode, bool includeFreeRunningChannels);

}  // namespace clockfw::core
