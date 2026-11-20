/**
 * @file control_panel.cpp
 * @brief HAL driver for the rotary encoder and three front-panel buttons.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/control_panel.h"

#include "hal/platform_io.h"

#include "pin_map.h"

#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
#include "hal/runtime_instrumentation.h"
#endif

namespace clockfw::hal {
namespace {

/** Debounce interval applied to active-low button inputs. */
constexpr std::uint32_t kDebounceMs = 25UL;

/**
 * A/B state in a detent (bit1 = A, bit0 = B, as platform::quadratureEncoderState()).
 * Bourns PEC11R datasheet REV. 04/26, p. 3, "Quadrature Output Table": at the detent
 * marked D both contacts are ON, i.e. closed to C. C is at GND and A/B have pull-ups,
 * so both read LOW. The fitted PEC11R-4220F-S0024 has 24 detents and 24 pulses: one
 * detent per quadrature cycle, every detent in this state.
 */
constexpr std::uint8_t kEncoderDetentState = 0b00U;

/** Contact bounce of the PEC11R, datasheet p. 1: "Contact Bounce (15 RPM) 2.0 ms max." */
constexpr std::uint32_t kEncoderContactBounceMs = 2UL;

/**
 * Quiet time before an anchor: no quadrature transition for this long. The
 * millisecond clock advances in whole ticks, so samples three ticks apart are more
 * than the 2 ms contact bounce apart.
 */
constexpr std::uint32_t kEncoderAnchorQuietMs = kEncoderContactBounceMs + 1UL;

}  // namespace

ControlPanel::DebouncedButton::DebouncedButton(const std::uint32_t pin) : pin_(pin) {}

void ControlPanel::DebouncedButton::begin(const std::uint32_t nowMs) {
    platform::configureInputPullup(pin_);
    rawPressed_ = !platform::read(pin_);
    stablePressed_ = rawPressed_;
    rawChangedAtMs_ = nowMs;
    transitionPending_ = false;
    transitionStartedAtUs_ = 0U;
    // A button held at boot has no observed first edge and no press timestamp.
    pressStartedAtUs_ = 0U;
    pressStartKnown_ = false;
}

ButtonSample ControlPanel::DebouncedButton::sample(const std::uint32_t nowMs) {
    const bool currentRawPressed = !platform::read(pin_);
    if (currentRawPressed != rawPressed_) {
        rawPressed_ = currentRawPressed;
        rawChangedAtMs_ = nowMs;
        // Only the first edge away from the stable state starts a transition;
        // bounce edges until it is accepted or settles back keep that timestamp.
        if (!transitionPending_ && rawPressed_ != stablePressed_) {
            transitionPending_ = true;
            transitionStartedAtUs_ = platform::microseconds();
        }
    }

    ButtonSample result{};
    const bool settled = nowMs - rawChangedAtMs_ >= kDebounceMs;
    if (rawPressed_ != stablePressed_ && settled) {
        stablePressed_ = rawPressed_;
        result.edge = stablePressed_ ? ButtonEdge::Pressed : ButtonEdge::Released;
        result.edgeTimestampUs = transitionStartedAtUs_;
        result.edgeTimestampValid = true;
        transitionPending_ = false;
        if (stablePressed_) {
            pressStartedAtUs_ = transitionStartedAtUs_;
            pressStartKnown_ = true;
        }
    } else if (rawPressed_ == stablePressed_ && settled) {
        // Back at the stable state for the full debounce time: the transition was
        // bounce or a press shorter than the debounce time and is discarded.
        transitionPending_ = false;
    }
    result.pressed = stablePressed_;
    result.pressTimestampValid =
        pressStartKnown_ && (stablePressed_ || result.edge == ButtonEdge::Released);
    result.pressTimestampUs = result.pressTimestampValid ? pressStartedAtUs_ : 0U;
    if (!stablePressed_) {
        pressStartKnown_ = false;
    }
    return result;
}

ControlPanel::ControlPanel()
    : encoderButton_(pinmap::kEncoderPushButtonPin), transportButton_(pinmap::kPlayPauseButtonPin),
      tapButton_(pinmap::kTapTempoButtonPin), resetButton_(pinmap::kResetBackButtonPin) {}

void ControlPanel::begin() {
    // PB6/PB7 are handled by the dedicated quadrature backend. On STM32F401 the
    // production implementation is TIM4 encoder mode (x4), so the first physical
    // detent and every later direction change are counted independently of EXTI
    // latency or foreground/display work.
    (void)platform::beginQuadratureEncoder(pinmap::kEncoderPhaseAPin, pinmap::kEncoderPhaseBPin);
    encoderLastTransitionCount_ = platform::quadratureEncoderCount();
    // Hardware Rev C fits a Bourns PEC11R-4220F-S0024: 24 detents / 24 PPR, one
    // quadrature cycle per mechanical detent. The detent state comes from the
    // datasheet (kEncoderDetentState), not from the state seen at boot (F-010).
    encoderTransitionRemainder_ = 0;
    pendingEncoderDetents_ = 0;
    encoderAnchorPending_ = false;

    const std::uint32_t nowMs = platform::milliseconds();
    encoderLastActivityMs_ = nowMs;
    encoderButton_.begin(nowMs);
    transportButton_.begin(nowMs);
    tapButton_.begin(nowMs);
    resetButton_.begin(nowMs);
}

ControlSample ControlPanel::sample(const std::uint32_t nowMs) {
    const ButtonSample encoderButton = encoderButton_.sample(nowMs);
    std::int8_t encoderDelta = sampleEncoder(nowMs);

    // The PEC11R push switch is part of the same mechanical shaft as A/B. Axial
    // push/release and contact bounce can leave the transition accumulator
    // between detents even when no intentional turn happened. The debounced
    // release requests an anchor; it is taken only at rest in the detent state,
    // where the partial residue is surely error (F-010).
    if (encoderButton.edge == ButtonEdge::Released) {
        requestEncoderAnchor();
    }
    anchorEncoderAtRest(nowMs);

    if (encoderDirectionReversed_) {
        encoderDelta = static_cast<std::int8_t>(-encoderDelta);
    }
    return {
        encoderDelta,
        encoderButton,
        transportButton_.sample(nowMs),
        tapButton_.sample(nowMs),
        resetButton_.sample(nowMs)};
}

void ControlPanel::setEncoderDirectionReversed(const bool reversed) {
    encoderDirectionReversed_ = reversed;
}

void ControlPanel::requestEncoderAnchor() {
    encoderAnchorPending_ = true;
    const std::uint8_t phase = platform::quadratureEncoderState();
#if defined(CLOCK_HOST_TEST)
    encoderDiagnostics_.anchorRequestPhase = phase;
#endif
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
    instrumentation::recordEncoderAnchorRequestPhase(phase);
#endif
    (void)phase;  // read only by the diagnostics
}

void ControlPanel::anchorEncoderAtRest(const std::uint32_t nowMs) {
    // Unsigned difference: exact across the 2^32 ms wrap.
    if (!encoderAnchorPending_ || nowMs - encoderLastActivityMs_ < kEncoderAnchorQuietMs ||
        platform::quadratureEncoderState() != kEncoderDetentState) {
        return;
    }
    encoderAnchorPending_ = false;
    // The count consumed by sampleEncoder() stays the baseline, so no transition
    // that arrives meanwhile is lost; only the partial residue is dropped.
    if (encoderTransitionRemainder_ != 0) {
        encoderTransitionRemainder_ = 0;
        countDiscardedEncoderRemainder();
    }
#if defined(CLOCK_HOST_TEST)
    ++encoderDiagnostics_.anchors;
#endif
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
    instrumentation::countEncoderAnchor();
#endif
}

void ControlPanel::countDiscardedEncoderRemainder() {
#if defined(CLOCK_HOST_TEST)
    ++encoderDiagnostics_.discardedRemainders;
#endif
#if defined(CLOCK_RUNTIME_INSTRUMENTATION)
    instrumentation::countEncoderDiscardedRemainder();
#endif
}

std::int8_t ControlPanel::sampleEncoder(const std::uint32_t nowMs) {
    constexpr std::int32_t kTransitionsPerDetent = 4;
    constexpr std::int16_t kMaximumPendingDetents = 32767;
    constexpr std::int16_t kMinimumPendingDetents = -32767;
    constexpr std::int16_t kMaximumReportedDelta = 127;
    constexpr std::int16_t kMinimumReportedDelta = -127;

    const std::uint32_t currentCount = platform::quadratureEncoderCount();
    const std::int32_t transitionDelta =
        static_cast<std::int32_t>(currentCount - encoderLastTransitionCount_);
    encoderLastTransitionCount_ = currentCount;

    if (transitionDelta != 0) {
        encoderLastActivityMs_ = nowMs;
        const std::int64_t accumulatedTransitions =
            static_cast<std::int64_t>(encoderTransitionRemainder_) + transitionDelta;
        const bool atDetentPhase = platform::quadratureEncoderState() == kEncoderDetentState;

        std::int32_t newDetents = 0;
        if (atDetentPhase) {
            // PEC11R-4220F-S0024 has one quadrature pulse per mechanical detent
            // (24 detents / 24 PPR). Returning to the datasheet detent state
            // is therefore a useful recovery boundary. Round a one-edge count
            // error to the nearest complete cycle and discard any remaining
            // half-cycle corruption instead of carrying it across a later
            // direction reversal. Ties (two transitions) round toward zero
            // because they are electrically ambiguous.
            const std::int64_t magnitude =
                accumulatedTransitions < 0 ? -accumulatedTransitions : accumulatedTransitions;
            const std::int32_t completedCycles =
                static_cast<std::int32_t>((magnitude + 1) / kTransitionsPerDetent);
            newDetents = accumulatedTransitions < 0 ? -completedCycles : completedCycles;
            encoderTransitionRemainder_ = 0;
            if (accumulatedTransitions !=
                static_cast<std::int64_t>(newDetents) * kTransitionsPerDetent) {
                countDiscardedEncoderRemainder();
            }
        } else {
            newDetents = static_cast<std::int32_t>(accumulatedTransitions / kTransitionsPerDetent);
            encoderTransitionRemainder_ = static_cast<std::int8_t>(
                accumulatedTransitions -
                static_cast<std::int64_t>(newDetents) * kTransitionsPerDetent);
        }

        const std::int32_t pending = static_cast<std::int32_t>(pendingEncoderDetents_) + newDetents;
        pendingEncoderDetents_ = static_cast<std::int16_t>(
            pending > kMaximumPendingDetents
                ? kMaximumPendingDetents
                : (pending < kMinimumPendingDetents ? kMinimumPendingDetents : pending));
    }

    if (pendingEncoderDetents_ == 0) {
        return 0;
    }

    const std::int16_t bounded = pendingEncoderDetents_ > kMaximumReportedDelta
        ? kMaximumReportedDelta
        : (pendingEncoderDetents_ < kMinimumReportedDelta ? kMinimumReportedDelta
                                                          : pendingEncoderDetents_);
    pendingEncoderDetents_ = static_cast<std::int16_t>(pendingEncoderDetents_ - bounded);
    return static_cast<std::int8_t>(bounded);
}

}  // namespace clockfw::hal
