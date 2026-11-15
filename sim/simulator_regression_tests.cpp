/**
 * @file simulator_regression_tests.cpp
 * @brief Native-simulator runtime regression tests for time advance and host-patch export.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "simulator_regression_tests.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>

#include "simulator_runtime.h"

namespace clockfw::sim::tests {
namespace {

int fail(const char* const message) {
    std::cerr << "simulator test failed: " << message << '\n';
    return 1;
}

}  // namespace

int runRuntimeRegressions() {
    // Issue #11: partial advances must carry the scheduler remainder. Wall time, input
    // time and engine time stay one time base however the host slices the advance.
    {
        const std::filesystem::path tickStatePath = ".clock-simulator-tick-test-state.bin";
        std::filesystem::remove(tickStatePath);
        SimulatorRuntime tickRuntime(tickStatePath);
        tickRuntime.begin();
        tickRuntime.advanceMicroseconds(1100000ULL);
        const auto ticksAfter = [&](const auto& advances) -> std::uint64_t {
            const std::uint64_t before = tickRuntime.schedulerTicksForTest();
            for (const std::uint64_t durationUs : advances) {
                tickRuntime.advanceMicroseconds(durationUs);
            }
            return tickRuntime.schedulerTicksForTest() - before;
        };
        const std::uint64_t startUs = tickRuntime.nowMicroseconds();
        std::array<std::uint64_t, 1000U> quarterTicks{};
        quarterTicks.fill(25ULL);
        if (ticksAfter(quarterTicks) != 500ULL ||
            tickRuntime.nowMicroseconds() - startUs != 25000ULL) {
            return fail("1000 x 25 us must fire 500 scheduler ticks and advance 25 ms");
        }
        // Remainders 0, 49, 50 and 51 us around the 50-us boundary.
        if (ticksAfter(std::array<std::uint64_t, 1U>{{0ULL}}) != 0ULL) {
            return fail("a zero advance must not fire the scheduler");
        }
        if (ticksAfter(std::array<std::uint64_t, 1U>{{49ULL}}) != 0ULL ||
            ticksAfter(std::array<std::uint64_t, 1U>{{1ULL}}) != 1ULL) {
            return fail("49 us + 1 us must complete exactly one scheduler tick");
        }
        if (ticksAfter(std::array<std::uint64_t, 1U>{{50ULL}}) != 1ULL) {
            return fail("50 us must fire exactly one scheduler tick");
        }
        if (ticksAfter(std::array<std::uint64_t, 1U>{{51ULL}}) != 1ULL ||
            ticksAfter(std::array<std::uint64_t, 1U>{{49ULL}}) != 1ULL) {
            return fail("51 us + 49 us must fire exactly two scheduler ticks");
        }
        // Large and irregular jumps: 4025 us repeatedly, then one jump with a remainder.
        std::array<std::uint64_t, 8U> irregular{};
        irregular.fill(4025ULL);
        if (ticksAfter(irregular) != 644ULL) {
            return fail("8 x 4025 us must fire 644 scheduler ticks");
        }
        if (ticksAfter(std::array<std::uint64_t, 1U>{{1000037ULL}}) != 20000ULL ||
            ticksAfter(std::array<std::uint64_t, 1U>{{13ULL}}) != 1ULL) {
            return fail("1000037 us + 13 us must fire 20001 scheduler ticks");
        }
        std::filesystem::remove(tickStatePath);
    }

    // Issue #9: a host patch saved while playing must carry the live settings. Flash
    // commits wait for STOP (D-04), so the exported image includes pending operations.
    {
        const std::filesystem::path patchStatePath = ".clock-simulator-patch-test-state.bin";
        std::filesystem::remove(patchStatePath);
        SimulatorRuntime patchRuntime(patchStatePath);
        patchRuntime.begin();
        patchRuntime.advanceMicroseconds(1100000ULL);
        const std::uint16_t patchStartBpm = patchRuntime.state().bpm;
        patchRuntime.setButton(SimButton::Play, true);
        patchRuntime.advanceMicroseconds(35000ULL);
        patchRuntime.setButton(SimButton::Play, false);
        patchRuntime.advanceMicroseconds(100000ULL);
        if (patchRuntime.state().transport != clockfw::TransportState::Playing) {
            return fail("patch test must save while PLAYING");
        }
        patchRuntime.rotateEncoder(5);
        patchRuntime.advanceMicroseconds(5000000ULL);
        const std::uint16_t liveBpm = patchRuntime.state().bpm;
        if (liveBpm != patchStartBpm + 5U) {
            return fail("five encoder detents must change the live tempo by five BPM");
        }
        const auto playingImage = patchRuntime.persistenceImage();
        patchRuntime.restorePersistenceImage(playingImage);
        patchRuntime.advanceMicroseconds(1100000ULL);
        if (patchRuntime.state().bpm != liveBpm) {
            return fail("a patch saved during PLAY must restore the live tempo");
        }

        // Immediately after an edit in STOP, before the three-second commit delay.
        patchRuntime.rotateEncoder(-2);
        patchRuntime.advanceMicroseconds(20000ULL);
        const std::uint16_t editedBpm = patchRuntime.state().bpm;
        const auto stoppedImage = patchRuntime.persistenceImage();
        patchRuntime.restorePersistenceImage(stoppedImage);
        patchRuntime.advanceMicroseconds(1100000ULL);
        if (editedBpm != liveBpm - 2U || patchRuntime.state().bpm != editedBpm) {
            return fail("a patch saved right after an edit must restore that edit");
        }
        std::filesystem::remove(patchStatePath);
    }

    return 0;
}

}  // namespace clockfw::sim::tests
