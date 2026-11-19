<!-- Author: Axel Napolitano | License: PolyForm-Noncommercial-1.0.0 -->

# South Signal Lab CLOCK Timing Contract

This document defines the firmware timing boundary. It distinguishes hard real-time musical behavior from foreground work such as display rendering, persistence, and menu handling.

## Hard real-time layer

CLOCK currently uses TIM3 as a **20 kHz scheduler interrupt**. One scheduler quantum is therefore **50 µs**. Gate scheduling, pulse termination, phase progression, external-sync consumption, and external-reset semantics execute from this scheduler path rather than from the foreground application loop.

The target interrupt hierarchy is deliberately asymmetric:

| Layer | Target priority | Responsibility |
| --- | ---: | --- |
| TIM3 scheduler | 0 | gate edges, pulse termination, phase, SYNC/RST consumption |
| GPIO EXTI | 4 | conditioned SYNC/RST edge capture |
| Foreground | n/a | UI, rendering, display service, persistence scheduling |

STM32 NVIC priorities use lower numbers for higher preemption priority. The scheduler therefore remains able to preempt display and input-service activity.

The current scheduler is service-tick based rather than final output-compare scheduling. Consequently the software timing quantization is 50 µs. A future compare-event scheduler may tighten that bound without changing the application timing contract.

### Gate output: one register write per tick

All gate edges of one scheduler tick are staged and written with **one GPIOB `BSRR` access** at the end of the tick, so edges that the engine places in the same tick leave the MCU pins together. A channel never appears in the set and the reset mask of the same write. (RM0368 Rev 6, 8.3.5 and 8.4.7: if both bits of one line are set, setting wins; the firmware never relies on that.) Foreground changes only ever lower gates and write at once, inside the interrupt lock that guards them.

Because one write cannot carry a fall and a rise of the same channel, a HIGH gate **ends one tick before its next rising edge** (next event or ratchet). Only when a reschedule in the same tick leaves no LOW tick does the rising edge move by one tick (50 µs). Tie continues the gate by design.

### Tick catch-up from TIM5

TIM3 and TIM5 count the same 1 MHz timer clock. Each scheduler interrupt computes the TIM5 timestamp of its TIM3 update (TIM5 minus the TIM3 counter) and advances the master timeline by the number of whole ticks since the previous update, rounded to the nearest tick. Updates that merged while interrupts were locked are therefore **caught up instead of lost**; the tempo stays exact. Events that fall into caught-up ticks fire in that interrupt, once each and in order. The count uses unsigned differences, so the TIM5 wrap at 2^32 µs is exact, and subtraction instead of division, so the work is bounded.

At most `kMaximumSchedulerCatchUpTicks` (8 ticks, 400 µs) are caught up in one interrupt; this matches the per-channel event bound of one tick. A longer stall drops the excess ticks rather than bursting events. Only a stall far beyond any interrupt lock reaches the bound, for example a Flash erase, which never runs while the engine plays.

### One time base

Milliseconds and microseconds derive from one source: a 64-bit extension of the 32-bit TIM5 counter. The scheduler interrupt is its only writer and folds the counter delta in every tick; readers in any context use a sequence number instead of an interrupt lock and read again if the interrupt ran in between. SysTick stops counting while a Flash operation stalls the CPU; TIM5 does not, so milliseconds no longer drift from microseconds. SysTick (`HAL_GetTick()`) remains only for HAL-internal timeouts and `HAL_Delay()`.

### Interrupt locks cover only the swap

A configuration change recomputes the next event of up to eight channels, with several 64-bit divisions each. That work runs in the foreground against a staged second configuration buffer and a short snapshot of the scheduler state; the interrupt lock covers only the swap, which exchanges the configuration pointer, installs the prepared values and divides nothing. If the scheduler moved a schedule basis meanwhile (an event of that channel, a restart, a phase correction, an external tempo or lock change), the preparation repeats; after three attempts the swap computes under the lock as before, so no change is lost. An event that the preparation placed between snapshot and swap fires in the next tick.

Custom Groove library and preview changes and Sequencer pattern changes use the same swap: the new pattern goes into a spare buffer and a copy of the groove table outside the lock, and the lock exchanges pointers. Sequencer step expressions are swapped the same way. No pattern or table is copied under the lock (`memcpy` is a byte loop in the release image).

PLAY, STOP and RESET restart the GLOBAL schedules on the current master position. Every term of an event position is relative to the epoch, so the first event of each GLOBAL channel is that position plus an offset that only the configuration and the tempo determine. The foreground computes these offsets and step phases outside the lock; the lock adds the position, installs them and lowers all HIGH gates with one register write. A tempo or lock change in between repeats the preparation, as above. A reset from the scheduler itself (RST input, end of Pre-Count, first SYNC pulse) prepares and installs in one step.

### External phase correction

Each accepted sync pulse snaps the GLOBAL grid onto the pulse. A late pulse moves the GLOBAL grid later. An early pulse moves the master timeline and every FREE schedule later instead of the GLOBAL grid earlier, which is the same relative correction; schedule positions therefore never move backwards and cannot saturate at the start of the timeline.

## Encoder and external inputs

The physical encoder and the external timing inputs deliberately use different capture paths.

The PEC11R A/B contacts are wired to PB6/PB7 and decoded by **TIM4 encoder mode**. The timer counts quadrature transitions in hardware; foreground control processing converts the wrapping transition count into mechanical detents and applies detent-phase resynchronization plus the user `NORMAL / REVERSED` semantic direction. The encoder therefore does not depend on GPIO EXTI delivery or display-loop latency.

The detent state comes from the encoder datasheet: in a detent both contacts are closed to the grounded common pin, so A and B read LOW (Bourns PEC11R, REV. 04/26, p. 3; 24 detents and 24 pulses, one detent per quadrature cycle). Each time the count reaches that state it is rounded to whole detents. Releasing the encoder push switch requests an anchor that drops a partial count left by the axial push; it is taken only when A and B rest in the detent state and no transition came for 3 ms, longer than the 2 ms contact bounce of the datasheet. A release while the shaft still moves or bounces therefore cannot fix a wrong phase.

Both conditioned comparator paths remain GPIO-interrupt driven (EXTI, priority 4). Their ISR work is limited to timestamping/queuing physical INPUT 1 / INPUT 2 levels; musical role interpretation happens in the 20 kHz scheduler. Factory roles remain SYNC/RESET, but either physical path may own a supported role. RESET processing retains precedence over SYNC when both assigned roles are pending at the same scheduler boundary; edge-controlled transport commands follow, with STOP winning simultaneous START/RESTART, and an assigned RUN level applied last as the authoritative transport state.

The current baseline timestamps conditioned comparator edges through GPIO EXTI; when one path owns the SYNC role, those timestamps feed the external-period estimator. Final hardware qualification must measure capture/output jitter under representative worst-case load. PA8 remains timer Input Capture capable, so hardware capture remains an escalation option if measured EXTI timing misses the requirement.

### Short triggers: parity rule

One EXTI pending bit stands for any number of edges. The interrupt clears it, reads the pin and compares the level with the last stored level:

- **Level changed:** an odd number of edges, counted as one edge of the new direction.
- **Level unchanged:** at least two edges merged, for example a trigger that was already over when the interrupt ran. The capture queues the missing opposite edge and then the observed edge, both with the same timestamp.

Trigger roles (START, STOP, RESTART, TAP, RESET as trigger) therefore fire exactly once for a short pulse; level roles (RUN, RESET as gate) follow only the level. Several pulses within one interrupt latency still count as one, which can only happen while the CPU is blocked. An edge between clearing the pending bit and reading the pin is already part of that level but sets the bit again; the interrupt clears it and reads again (at most three times), so the next interrupt does not count it a second time. The synthesized and the observed edge share one timestamp; the tempo estimator rejects the resulting zero period, so neither can cause a tempo step.

The edge queue between the EXTI handler (producer) and the scheduler interrupt (consumer, which preempts the producer) publishes each slot with a release store of its head index and reads it after an acquire load, so the consumer never sees a slot before its contents.

### Input timestamp jitter

The timestamp is the TIM5 microsecond counter read in the EXTI handler; the scheduler interrupt (priority 0) and foreground interrupt locks can delay it. With the internal budget of at most 20 µs for the scheduler interrupt and 10 µs for an interrupt lock, the firmware contributes at most **30 µs** of input timestamp jitter (calculated, not yet measured; the comparator stage adds its own share). All gate outputs sit on the 50 µs scheduler grid and the tempo estimator averages, so this stays below one scheduler quantum at the outputs.

Flash erase runs only with the engine stopped (D-04) and stalls the CPU. A trigger during an erase is not lost: the pending bit holds it, and the parity rule counts it when the erase has finished. It arrives late, by up to the rest of the erase.

The scheduler interrupt changes transport only through the lock-free `playFromIsr()`/`stopFromIsr()` variants; `scripts/check_architecture.py` reports an `InterruptLock` or a call to a locking variant on any path from an interrupt entry point. Closing the Flash commit window starts a deferred START outside any lock; the engine swaps the prepared start in under its own lock, and in that same lock the window closes.

## Groove and TAP-record timing

Groove is a deterministic timing layer on the existing master timeline, not a second clock. Factory Groove presets and named Custom Grooves ultimately resolve to bounded per-step offsets. `AMOUNT` scales the stored offset and `ROTATE` changes which stored step is applied to the current event. In Independent mode the owning channel supplies the Groove; One Clock uses one shared Groove; Divider Bank bypasses Groove entirely. One Clock Humanize remains a separate deterministic per-output displacement and is not folded into stored Groove data.

Custom Groove offsets are signed and represented relative to the nominal step interval. The engine clamps effective event positions so timestamps remain monotonic and a marker cannot overtake the preceding or following event. Gate-off scheduling is still bounded against the resulting effective interval, so deep Groove cannot create a rising-edge/gate-off inversion. Straight timing, classic Swing and Groove therefore share the same scheduler invariants.

### Swing, Groove, and Humanize are distinct layers

Classic Swing is an alternating long/short displacement. Groove is a repeating deterministic per-step pattern with Amount and Rotate, including signed early/late offsets for Custom Grooves. Humanize is a separate One Clock-only deterministic per-output displacement and is never serialized into Groove data. The engine applies Swing + Groove shaping before Humanize, then bounds the effective result so event order and the scheduler-quantum safety margin are preserved.

`GROOVE → RECORD` captures the front-panel TAP TEMPO button against the live Q32 musical position. The physical button layer preserves the original press timestamp before debounce; after the press is validated, the recorder uses that high-resolution timestamp instead of the later debounced foreground time. TAP is consumed by the recorder and is not sent to Tap Tempo while the Record screen is active.

Recorder Count-In is independent from the global transport Pre-Count. It gates **capture readiness**, not the master transport configuration. `ONE SHOT` stops at the end of the selected 1–64-step pattern; `ENDLESS` wraps and replaces only steps receiving a new TAP. PLAY from the Record screen controls recorder start/stop, while PLAY+TURN is consumed as a zoom chord and must not issue a transport command. The visible playhead is derived from the same engine phase used for capture, so UI and stored offsets share one musical reference.

Host tests verify timestamp preservation, step assignment, One-Shot/Endless behavior and monotonic scheduling. Physical switch latency, contact behavior and end-to-end TAP-to-gate jitter remain HIL measurements (`HIL-GRV-004`).

## Display transports

Final hardware uses SPI. The legacy I2C transport is retained only as a host regression path and is not a production Blackpill profile.

### SPI — reference transport

SPI is the default PlatformIO/reference profile. `present()` transfers dirty pages immediately. The dedicated bus is fast enough that this remains the simplest path and it does not inherit the additional scheduling machinery required by I2C.

SPI clock (calculated, not measured): SPI1 runs from PCLK2. The DIY Blackpill build has a 25 MHz HSE; `configureSystemClock()` sets PLLM 25, PLLN 336, PLLP 4 (SYSCLK 84 MHz) and APB2 ÷1, so PCLK2 is 84 MHz (RM0368 Rev 6, 6.3.2). `CLOCK_DISPLAY_SPI_FREQUENCY_HZ` requests 1 MHz; the fastest SPI_CR1 BR divider at or below that is ÷128 (RM0368 Rev 6, 20.5.1), so SCK runs at **656.25 kHz**. A full 1024-byte frame then takes **12.5 ms** of bit time (8192 bits), plus command bytes and the gaps between bytes; `HAL_SPI_Transmit()` has a 20 ms timeout. A failed transfer leaves its pages dirty, and the next frame sends them again. The SMD variant has an 8 MHz HSE and needs other PLL values (hardware reference); the figures above apply to the DIY build. The measurement of SCK and frame time on the scope is planned for 1.2.1.

### Legacy I2C transport — regression/reference only

I2C is no longer a supported production wiring option because PB6/PB7 are reserved for the encoder. The implementation remains regression-tested so older transport logic does not silently rot; blocking STM32Cube I2C master transmissions can keep the foreground waiting for transfer completion, so CLOCK never sends a complete 1 KiB framebuffer from one runtime `present()` call.

Instead:

1. `present()` publishes the newest 1 KiB framebuffer as the current target and performs **no runtime I2C transfer**.
2. `OledDisplay::service()` performs at most **one I2C transaction per foreground iteration**.
3. Display data transactions contain at most **24 framebuffer bytes** plus the SSD13xx control byte.
4. Dirty pages are transferred incrementally.
5. If a newer frame is published before the old one finishes, the newer frame becomes the target immediately. UI frames may therefore be skipped; musical events may not.

At 400 kHz, a 24-byte payload transaction occupies the bus for well below one millisecond before software overhead. This is a bounded foreground cost rather than a multi-page framebuffer stall. The exact physical timing remains a HIL measurement, not a host-test claim.

Controller initialization may still flush the initial blank framebuffer synchronously because this happens before the scheduler and gate-output stage are enabled.

Small OLED commands such as contrast/power changes remain synchronous single-command transactions. They are short and remain preemptible by the scheduler.

## Why no RTOS

The STM32F401 is single-core. An RTOS would provide scheduling abstraction, not simultaneous execution. The hard timing work already lives in hardware interrupts, while display work is explicitly bounded and discardable. Introducing an RTOS would add stack/RAM usage, priority interactions, and synchronization paths without solving a problem that requires a second execution core.

If physical HIL shows unacceptable I2C interference despite the bounded service path, the next escalation is a dedicated **HAL interrupt/DMA display transport**, not an RTOS. That change must preserve the same application-level `present()`/`service()` contract.

## Timing invariants

The following invariants are release requirements:

- display transport must not change generated clock frequency;
- no gate edge may be omitted because a display transfer is active;
- all gate edges of one scheduler tick are written with one `BSRR` access, and a HIGH gate ends before it rises again;
- scheduler ticks merged during an interrupt lock are caught up from TIM5 (bounded, see above), so the master tempo does not depend on foreground locks;
- milliseconds and microseconds come from the same TIM5 time base;
- foreground interrupt locks cover copying and swapping only, not schedule calculations, and copy no structure byte by byte;
- configured gate lengths remain scheduler-quantized but transport-independent;
- TIM4 encoder transitions must continue accumulating while foreground display work is active, and foreground detent conversion must not lose a complete mechanical detent;
- conditioned SYNC/RST edges must remain capturable while OLED traffic is active, and a trigger shorter than the interrupt latency fires its role exactly once;
- interrupt code opens no interrupt lock;
- legacy I2C regression builds may lower visual frame rate or skip stale frames under load;
- persistence writes remain prohibited while the engine runs because STM32F4 Flash programming can stall instruction/data fetches; immediately before each erase the commit gate checks the engine and opens a commit window under one interrupt lock, and scheduler-initiated starts (RUN/START/RESTART, auto-start on lock) inside that window are deferred until the commit has finished;
- Groove/Swing offsets must preserve monotonic event order and cannot move a rising event beyond its neighboring event boundary;
- Groove Record must preserve the physical TAP press timestamp through debounce and must never route a recorder TAP into Tap Tempo.

## Verification layers

Host tests prove the state-machine and scheduling contract: fixed-point timing, gate lengths, SYNC/RST semantics, TIM4-style wrapping encoder accumulation/detent recovery, I2C transaction bounds, latest-frame-wins behavior, and SPI/I2C build equivalence. Physical encoder contact behavior remains a HIL concern.

Only HIL can establish actual edge jitter, IRQ latency, SPI electrical behavior, comparator thresholds, and the final timing distribution at the jacks. The required bench matrix is defined in [`HIL_TEST_PLAN.md`](HIL_TEST_PLAN.md).
