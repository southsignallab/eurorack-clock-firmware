<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# 24 Technical status and specifications

The current firmware uses a deterministic **20 kHz scheduler**, giving a 50 µs service quantum. UI rendering and I/O transport do not decide musical gate timing. The display is connected over SPI, the only production display path; PB6/PB7 belong to the encoder, so no firmware environment builds an I2C display profile. A legacy I2C transport with deferred, bounded foreground transactions remains in the source only as a regression path. The physical encoder uses PB6/PB7 in STM32 TIM4 encoder mode; SYNC/RST remain interrupt-captured inputs.

Persistence uses internal STM32 Flash A/B records. Large persistence staging buffers are static rather than runtime-stack allocations, embedded production code is guarded against dynamic heap allocation, and the build includes a production stack-frame gate.

These software safeguards do not replace physical validation. Representative hardware still needs oscilloscope/logic-analyzer proof for gate jitter and pulse widths, boot/reset behavior, SYNC/RST comparator behavior, EXTI capture timing, and SPI display stress. Until 1.5.0 this evidence is tracked but does not block the automated release build. See [`HIL_TEST_PLAN.md`](../HIL_TEST_PLAN.md).

### Licensing

Firmware source is licensed under the **PolyForm Noncommercial License 1.0.0**. The Required Notice is `Required Notice: Copyright © 2026 Axel Napolitano.` See [`../LICENSE.md`](../../LICENSE.md), [`../NOTICE.txt`](../../NOTICE.txt), and [`LICENSING.md`](../LICENSING.md). The publication manual and documentation artwork use the documentation license described in [`manual-source/LICENSE.md`](../manual-source/LICENSE.md). Third-party components retain their upstream licenses and notices.
