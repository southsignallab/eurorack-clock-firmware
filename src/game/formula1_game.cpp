/**
 * @file formula1_game.cpp
 * @brief Boot-only monochrome pseudo-3D racer Easter egg implementation.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "game/formula1_game.h"
#include "game/boot_game_controls.h"
#include "game/formula1_track.h"

#include <algorithm>
#include <array>

#include "hal/system_clock.h"

namespace clockfw::game {
namespace {
constexpr std::uint32_t kFrameIntervalMs = 45U;
constexpr std::uint32_t kCrashDurationMs = 820U;
}  // namespace

using formula1::clampToRange;
using formula1::kHorizonY;
using formula1::kPlayerBodyHeight;
using formula1::kPlayerBodyWidth;
using formula1::kPlayerY;
using formula1::kRoadBottomY;
using formula1::laneCenterAt;
using formula1::roadLeftAt;
using formula1::roadRightAt;
using formula1::trafficCarHeightAt;
using formula1::trafficCarWidthAt;

Formula1Game::Formula1Game(
    hal::OledDisplay& display,
    hal::ControlPanel& controls,
    hal::GateOutputDriver& gateOutputs,
    ArcadeLeaderboardStore& leaderboard)
    : display_(display), controls_(controls), gateOutputs_(gateOutputs),
      shell_(display, ArcadeTitle::Formula1, &leaderboard) {}

void Formula1Game::run() {
    gateOutputs_.disableOutputStage();
    gateOutputs_.setAllChannelsLow();
    const std::uint32_t startedAtMs = hal::SystemClock::milliseconds();
    shell_.begin(startedAtMs);
#ifdef CLOCK_HOST_TEST
    shell_.startImmediatelyForTest();
    highScore_.score = shell_.highScore();
    resetSession();
    render();
    return;
#else
    std::uint32_t lastFrameAtMs = startedAtMs;
    while (!exitRequested_) {
        const std::uint32_t nowMs = hal::SystemClock::milliseconds();
        const hal::ControlSample controls = controls_.sample(nowMs);
        if (exitHold_.update(controls.encoderButton, nowMs, kExitLongPressMs)) {
            exitRequested_ = true;
        }
        const ArcadeShell::Action action = shell_.update(controls, nowMs);
        if (action == ArcadeShell::Action::StartRun || action == ArcadeShell::Action::RestartRun) {
            highScore_.score = shell_.highScore();
            resetSession();
        }
        if (shell_.playing()) {
            update(controls, nowMs);
        }
        if (nowMs - lastFrameAtMs >= kFrameIntervalMs) {
            if (shell_.playing()) {
                render();
            } else {
                shell_.render(nowMs);
            }
            lastFrameAtMs = nowMs;
        }
        (void)display_.service();  // only reports pending I2C work
        hal::SystemClock::delayMilliseconds(1U);
    }
    gateOutputs_.setAllChannelsLow();
    gateOutputs_.disableOutputStage();
#endif
}

#ifdef CLOCK_SIMULATOR
void Formula1Game::beginForSimulator() {
    gateOutputs_.disableOutputStage();
    gateOutputs_.setAllChannelsLow();
    const std::uint32_t nowMs = hal::SystemClock::milliseconds();
    shell_.begin(nowMs);
    lastSimulatorFrameAtMs_ = nowMs;
    shell_.render(nowMs);
}
bool Formula1Game::serviceForSimulator(const std::uint32_t nowMs) {
    const hal::ControlSample controls = controls_.sample(nowMs);
    if (exitHold_.update(controls.encoderButton, nowMs, kExitLongPressMs)) {
        exitRequested_ = true;
    }
    const ArcadeShell::Action action = shell_.update(controls, nowMs);
    if (action == ArcadeShell::Action::StartRun || action == ArcadeShell::Action::RestartRun) {
        highScore_.score = shell_.highScore();
        resetSession();
    }
    if (shell_.playing()) {
        update(controls, nowMs);
    }
    if (nowMs - lastSimulatorFrameAtMs_ >= kFrameIntervalMs) {
        if (shell_.playing()) {
            render();
        } else {
            shell_.render(nowMs);
        }
        lastSimulatorFrameAtMs_ = nowMs;
    }
    if (exitRequested_) {
        gateOutputs_.setAllChannelsLow();
        gateOutputs_.disableOutputStage();
    }
    return !exitRequested_;
}
#endif

void Formula1Game::resetSession() {
    for (Car& car : traffic_) {
        car = {};
    }
    playerX_ = 63;
    speed_ = 92U;
    distance_ = 0U;
    elapsedMs_ = 0U;
    score_ = 0U;
    lap_ = 1U;
    crashesRemaining_ = 3U;
    lastPhysicsAtMs_ = 0U;
    physicsStarted_ = false;
    lastSpawnAtMs_ = 0U;
    exitHold_.cancel();
    crashStartedAtMs_ = 0U;
    crashed_ = false;
    gameOver_ = false;
    scoreFinalized_ = false;
    exitRequested_ = false;
    spawnTraffic();
    traffic_[0].y = 18;
    spawnTraffic();
    traffic_[1].y = 27;
}

std::uint32_t Formula1Game::nextRandom() {
    randomState_ ^= randomState_ << 13U;
    randomState_ ^= randomState_ >> 17U;
    randomState_ ^= randomState_ << 5U;
    return randomState_;
}
void Formula1Game::spawnTraffic() {
    constexpr std::array<std::int8_t, 3U> lanes{{-1, 0, 1}};
    for (Car& car : traffic_) {
        if (!car.active) {
            car.lane = lanes[nextRandom() % lanes.size()];
            car.y = kHorizonY + 1;
            car.active = true;
            return;
        }
    }
}
void Formula1Game::startCrash(const std::uint32_t nowMs) {
    crashed_ = true;
    crashStartedAtMs_ = nowMs;
    speed_ = 0U;
    if (crashesRemaining_ > 0U) {
        --crashesRemaining_;
    }
    score_ = score_ > 100U ? score_ - 100U : 0U;
}

void Formula1Game::update(const hal::ControlSample& controls, const std::uint32_t nowMs) {
    if (gameOver_) {
        return;
    }
    if (crashed_) {
        if (nowMs - crashStartedAtMs_ >= kCrashDurationMs) {
            crashed_ = false;
            if (crashesRemaining_ == 0U) {
                gameOver_ = true;
                finishScore();
                return;
            }
            for (Car& car : traffic_) {
                car = {};
            }
            speed_ = 96U;
            spawnTraffic();
            traffic_[0].y = 18;
        }
        return;
    }
    if (controls.encoderDelta != 0) {
        const std::int16_t leftLimit =
            static_cast<std::int16_t>(roadLeftAt(distance_, kPlayerY + 6) + 6);
        const std::int16_t rightLimit =
            static_cast<std::int16_t>(roadRightAt(distance_, kPlayerY + 6) - 6);
        playerX_ = clampToRange(
            static_cast<std::int16_t>(
                playerX_ + static_cast<std::int16_t>(controls.encoderDelta) * 3),
            leftLimit,
            rightLimit);
    }
    if (nowMs - lastPhysicsAtMs_ < kFrameIntervalMs) {
        return;
    }
    // A frame at 0 ms is a frame like any other; only the first one has no predecessor (F-074).
    elapsedMs_ += physicsStarted_ ? nowMs - lastPhysicsAtMs_ : kFrameIntervalMs;
    lastPhysicsAtMs_ = nowMs;
    physicsStarted_ = true;

    const std::uint32_t speedPhase = (distance_ / 300U) % 6U;
    const std::uint16_t target = (speedPhase == 1U || speedPhase == 4U)
        ? 166U
        : ((speedPhase == 2U || speedPhase == 5U) ? 142U : 184U);
    if (speed_ < target) {
        speed_ = static_cast<std::uint16_t>(
            std::min<std::uint16_t>(target, static_cast<std::uint16_t>(speed_ + 3U)));
    } else if (speed_ > target) {
        speed_ = static_cast<std::uint16_t>(speed_ - 2U);
    }
    distance_ += static_cast<std::uint32_t>(1U + speed_ / 55U);
    score_ = std::min<std::uint32_t>(999999U, score_ + 1U + speed_ / 72U);
    lap_ = static_cast<std::uint8_t>(1U + (distance_ / 2600U) % 9U);

    if (nowMs - lastSpawnAtMs_ > 1050U + (nextRandom() % 700U)) {
        spawnTraffic();
        lastSpawnAtMs_ = nowMs;
    }
    const std::int16_t advance = static_cast<std::int16_t>(1 + speed_ / 90U);
    const std::int16_t playerLeft = static_cast<std::int16_t>(playerX_ - kPlayerBodyWidth / 2);
    const std::int16_t playerRight = static_cast<std::int16_t>(playerLeft + kPlayerBodyWidth - 1);
    for (Car& car : traffic_) {
        if (!car.active) {
            continue;
        }
        car.y = static_cast<std::int16_t>(car.y + advance);
        if (car.y > kRoadBottomY + 8) {
            car.active = false;
            score_ = std::min<std::uint32_t>(999999U, score_ + 35U);
            continue;
        }
        const std::int16_t centerX =
            laneCenterAt(distance_, car.lane, static_cast<std::int16_t>(car.y + 2));
        const std::int16_t width = trafficCarWidthAt(car.y);
        const std::int16_t height = trafficCarHeightAt(car.y);
        const std::int16_t left = static_cast<std::int16_t>(centerX - width / 2);
        const std::int16_t right = static_cast<std::int16_t>(left + width - 1);
        const std::int16_t bottom = static_cast<std::int16_t>(car.y + height + 1);
        if (right >= playerLeft && left <= playerRight && bottom >= kPlayerY &&
            car.y <= kPlayerY + kPlayerBodyHeight) {
            car.active = false;
            startCrash(nowMs);
            break;
        }
    }
}

void Formula1Game::finishScore() {
    if (scoreFinalized_) {
        return;
    }
    shell_.finishRun(score_);
    highScore_.score = std::max<std::uint32_t>(highScore_.score, score_);
    scoreFinalized_ = true;
}

}  // namespace clockfw::game
