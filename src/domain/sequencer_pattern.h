/**
 * @file sequencer_pattern.h
 * @brief Sequencer 2.0 pattern model and deterministic traversal helpers.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace clockfw {

/** Number of user pattern slots owned by each physical sequencer channel. */
inline constexpr std::uint8_t kSequencerPatternSlotsPerChannel = 8U;

/** Maximum Sequencer 2.0 pattern length; the OLED editor is only a viewport. */
inline constexpr std::uint8_t kSequencerMaximumSteps = 128U;

/** Two 64-bit words store the 128 binary gate states without heap allocation. */
inline constexpr std::size_t kSequencerGateWordCount = 2U;

/** Traversal order used when mapping timeline events onto pattern steps. */
enum class SequencerPlayDirection : std::uint8_t {
    Forward = 0U,
    Reverse = 1U,
    PingPong = 2U,
    Random = 3U,
};

/** End-of-pattern behavior. */
enum class SequencerLoopMode : std::uint8_t {
    Loop = 0U,
    Once = 1U,
};

/**
 * @brief One persistent Sequencer 2.0 gate pattern.
 *
 * Per-step probability/gate/tie/ratchet metadata is intentionally not embedded
 * here. It lives in the separate sparse override area so an untouched 128-step
 * pattern remains compact.
 */
struct SequencerPatternV2 final {
    /** Pattern length in steps, 1..kSequencerMaximumSteps. */
    std::uint8_t length = 16U;
    /** Rotation in steps, below length. */
    std::uint8_t rotation = 0U;
    /** Traversal order. */
    SequencerPlayDirection direction = SequencerPlayDirection::Forward;
    /** End-of-pattern behavior. */
    SequencerLoopMode loopMode = SequencerLoopMode::Loop;
    /** Gate bits: word 0 holds steps 0..63, word 1 steps 64..127 (bit n = step n). */
    std::array<std::uint64_t, kSequencerGateWordCount> gates{{0x1111ULL, 0ULL}};
};

/** Result of mapping one timeline event onto a pattern step. */
struct SequencerTraversalResult final {
    /** False for an invalid pattern or after the cycle of a ONCE pattern. */
    bool active = false;
    /** Zero-based step before rotation; valid while active is set. */
    std::uint8_t step = 0U;
};

/**
 * @brief Returns true when every bounded field in the pattern is valid.
 * @param pattern Pattern to check.
 * @return True for length 1..kSequencerMaximumSteps, rotation below length and a known
 *         direction and loop mode.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
bool isSequencerPatternValid(const SequencerPatternV2& pattern);

/**
 * @brief Returns whether one zero-based step contains a gate; out-of-range steps are false.
 * @param pattern Pattern to read.
 * @param step Zero-based step, 0..127; the pattern length is not checked.
 * @return True when the step's gate bit is set.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
bool sequencerPatternGate(const SequencerPatternV2& pattern, std::uint8_t step);

/**
 * @brief Sets or clears one zero-based step; returns false when the step is outside 0..127.
 * @param pattern Pattern to change.
 * @param step Zero-based step, 0..127; the pattern length is not checked.
 * @param enabled True sets the gate, false clears it.
 * @return False when @p step is out of range and nothing changed.
 */
bool setSequencerPatternGate(SequencerPatternV2& pattern, std::uint8_t step, bool enabled);

/**
 * @brief Clears any gate bits beyond the current active pattern length.
 * @param pattern Pattern to bound; a rotation at or beyond the length becomes 0, and
 *        length 0 clears every gate.
 */
void clampSequencerPattern(SequencerPatternV2& pattern);

/**
 * @brief Number of event positions in one traversal cycle.
 *
 * Forward, Reverse, and Random use LENGTH events. PingPong omits duplicate end
 * points and therefore uses 2*LENGTH-2 events for lengths greater than one.
 *
 * @param pattern Pattern; a length of 0 is treated as 1.
 * @return Number of events in one cycle.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
std::uint16_t sequencerTraversalCycleLength(const SequencerPatternV2& pattern);

/**
 * @brief Maps a monotonically increasing event serial onto a pattern step.
 *
 * RANDOM uses a deterministic stateless hash of eventSerial and randomSeed. The
 * caller may choose a new seed for a new transport run while reschedules inside
 * that run remain reproducible because they do not depend on call history.
 * ONCE returns active=false after one complete traversal cycle.
 *
 * @param eventSerial Event serial on the channel grid.
 * @param pattern Pattern; an invalid pattern gives active=false.
 * @param randomSeed Seed of the RANDOM direction.
 * @return Step before rotation, and whether the event addresses one.
 *
 * @isr TIM3 scheduler interrupt: constant time (one 64-bit modulo), non-blocking, no
 *      allocation, opens no interrupt lock.
 */
SequencerTraversalResult resolveSequencerTraversal(
    std::uint64_t eventSerial,
    const SequencerPatternV2& pattern,
    std::uint32_t randomSeed);

/**
 * @brief Applies the pattern's circular rotation to a resolved zero-based step.
 * @param step Zero-based step before rotation.
 * @param pattern Pattern; an invalid pattern gives 0.
 * @return (step + rotation) modulo the length.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
std::uint8_t rotateSequencerStep(std::uint8_t step, const SequencerPatternV2& pattern);

/**
 * @brief Resolves traversal + rotation and returns whether the addressed event emits a
 *        binary gate.
 * @param eventSerial Event serial on the channel grid.
 * @param pattern Pattern to evaluate.
 * @param randomSeed Seed of the RANDOM direction.
 * @return True when the event is active and its rotated step has a gate.
 */
bool sequencerPatternHitForEvent(
    std::uint64_t eventSerial,
    const SequencerPatternV2& pattern,
    std::uint32_t randomSeed);

}  // namespace clockfw
