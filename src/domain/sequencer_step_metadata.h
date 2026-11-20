/**
 * @file sequencer_step_metadata.h
 * @brief Compact Sequencer 2.0 per-step override model.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <cstdint>

namespace clockfw {

/** Packed real-time representation used by the scheduler: no compiler bitfields. */
using SequencerStepMetadataWord = std::uint16_t;

/** Gate-duration override applied to one Sequencer step. */
enum class SequencerGateProfile : std::uint8_t {
    Default = 0U,
    Trigger1Ms,
    Trigger2Ms,
    Trigger5Ms,
    Trigger10Ms,
    Duty25,
    Duty50,
    Duty75,
    Gate20Ms,
    Gate50Ms,
    Gate100Ms,
};

/**
 * @brief Optional expression attached to one zero-based Sequencer step.
 *
 * Probability 0 means inherit the channel value. Ratchet count 1 means no
 * ratchet. DEFAULT gate profile inherits the channel gate length. Tie and
 * ratchet are mutually exclusive when normalized for runtime use.
 */
struct SequencerStepMetadata final {
    /** Step probability in percent, 0..100; 0 inherits the channel value. */
    std::uint8_t probabilityPercent = 0U;
    /** Gate-duration override; DEFAULT inherits the channel gate length. */
    SequencerGateProfile gateProfile = SequencerGateProfile::Default;
    /** Sub-events of the step, 1..kSequencerMaximumRatchetCount; 1 means no ratchet. */
    std::uint8_t ratchetCount = 1U;
    /** True holds the gate into the next step. */
    bool tie = false;
};

/** Maximum bounded EVEN ratchet count supported by the Sequencer step model. */
inline constexpr std::uint8_t kSequencerMaximumRatchetCount = 8U;

/**
 * @brief Returns true when every encoded step-override value is valid.
 * @param metadata Record to check.
 * @return True for probability 0..100, a known gate profile and a ratchet count of
 *         1..kSequencerMaximumRatchetCount; tie is not checked.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
bool isSequencerStepMetadataValid(const SequencerStepMetadata& metadata);

/**
 * @brief Returns true when a step inherits all channel defaults and has no expression
 *        override.
 * @param metadata Record to check.
 * @return True for probability 0, DEFAULT gate, ratchet count 1 and no tie.
 */
bool isSequencerStepMetadataDefault(const SequencerStepMetadata& metadata);

/**
 * @brief Packs one validated metadata record into the stable 16-bit runtime representation.
 * @param metadata Record to pack.
 * @return Packed word: probability bits 0..6, gate profile bits 7..10, ratchet count - 1
 *         bits 11..13, tie bit 14, bit 15 reserved; 0 for an invalid record.
 */
SequencerStepMetadataWord packSequencerStepMetadata(const SequencerStepMetadata& metadata);

/**
 * @brief Decodes one stable 16-bit runtime word; invalid/reserved words decode to defaults.
 * @param word Packed word.
 * @return Decoded record, or the default record for a word with bit 15 set or an invalid
 *         field.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
SequencerStepMetadata unpackSequencerStepMetadata(SequencerStepMetadataWord word);

/**
 * @brief Returns a relative duty percentage, or zero for DEFAULT/absolute trigger profiles.
 * @param profile Gate profile.
 * @return 25, 50 or 75 for the duty profiles, otherwise 0.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
std::uint8_t sequencerGateProfileDutyPercent(SequencerGateProfile profile);

/**
 * @brief Returns an absolute gate duration in ms, inheriting fallbackMs for DEFAULT/duty
 *        profiles.
 * @param profile Gate profile.
 * @param fallbackMs Channel gate length in milliseconds.
 * @return Gate length in milliseconds.
 *
 * @isr TIM3 scheduler interrupt: constant time, non-blocking, no allocation, opens no
 *      interrupt lock.
 */
std::uint16_t sequencerGateProfileMilliseconds(
    SequencerGateProfile profile,
    std::uint16_t fallbackMs);

}  // namespace clockfw
