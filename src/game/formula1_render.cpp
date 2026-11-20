/**
 * @file formula1_render.cpp
 * @brief Frame rendering of the Formula 1 boot game: scenery, road, cars and HUD.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "game/formula1_game.h"
#include "game/formula1_track.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "text_format.h"
#include "ui_text.h"

namespace clockfw::game {
namespace {
using formula1::kHorizonY;
using formula1::kPlayerBodyWidth;
using formula1::kPlayerY;
using formula1::kRoadBottomY;
using formula1::laneCenterAt;
using formula1::roadCenterAt;
using formula1::roadLeftAt;
using formula1::roadRightAt;
using formula1::roadWidthAt;
using formula1::trafficCarHeightAt;
using formula1::trafficCarWidthAt;

struct MiniTrackPoint final {
    std::int8_t x;
    std::int8_t y;
};

constexpr std::array<MiniTrackPoint, 18U> kMiniTrack{
    {{-10, 4},
     {-12, 1},
     {-11, -3},
     {-7, -5},
     {-3, -5},
     {0, -2},
     {3, -5},
     {8, -5},
     {11, -2},
     {11, 2},
     {8, 5},
     {4, 5},
     {1, 3},
     {-2, 5},
     {-6, 5},
     {-10, 4},
     {-12, 1},
     {-10, 4}}};

void drawCloud(hal::OledDisplay& display, const std::int16_t x, const std::int16_t y) {
    display.drawHorizontalLine(static_cast<std::int16_t>(x + 1), y, 4);
    display.drawHorizontalLine(x, static_cast<std::int16_t>(y + 1), 7);
    display.drawHorizontalLine(
        static_cast<std::int16_t>(x + 2), static_cast<std::int16_t>(y + 2), 5);
}
void drawMountains(hal::OledDisplay& display) {
    display.drawLine(2, 11, 10, 7);
    display.drawLine(10, 7, 18, 11);
    display.drawLine(18, 11, 24, 8);
    display.drawLine(24, 8, 30, 11);
    display.drawLine(98, 11, 106, 8);
    display.drawLine(106, 8, 114, 11);
    display.drawLine(114, 11, 122, 7);
    display.drawLine(122, 7, 127, 11);
}
void drawPlayerCar(hal::OledDisplay& display, const std::int16_t centerX) {
    const std::int16_t x = static_cast<std::int16_t>(centerX - kPlayerBodyWidth / 2);
    const std::int16_t y = kPlayerY;
    display.drawLine(
        centerX, y, static_cast<std::int16_t>(x + 2), static_cast<std::int16_t>(y + 3));
    display.drawLine(
        centerX, y, static_cast<std::int16_t>(x + 9), static_cast<std::int16_t>(y + 3));
    display.fillRectangle(static_cast<std::int16_t>(x + 3), static_cast<std::int16_t>(y + 3), 6, 2);
    display.drawRectangle(static_cast<std::int16_t>(x + 2), static_cast<std::int16_t>(y + 5), 8, 2);
    display.fillRectangle(x, static_cast<std::int16_t>(y + 3), 2, 3);
    display.fillRectangle(
        static_cast<std::int16_t>(x + 10), static_cast<std::int16_t>(y + 3), 2, 3);
    display.fillRectangle(static_cast<std::int16_t>(x + 1), static_cast<std::int16_t>(y + 7), 2, 2);
    display.fillRectangle(static_cast<std::int16_t>(x + 9), static_cast<std::int16_t>(y + 7), 2, 2);
}
void drawTrafficCar(hal::OledDisplay& display, const std::int16_t centerX, const std::int16_t y) {
    const std::int16_t width = trafficCarWidthAt(y);
    const std::int16_t height = trafficCarHeightAt(y);
    const std::int16_t x = static_cast<std::int16_t>(centerX - width / 2);
    display.drawLine(
        centerX, y, static_cast<std::int16_t>(x + 1), static_cast<std::int16_t>(y + 1));
    display.drawLine(
        centerX, y, static_cast<std::int16_t>(x + width - 2), static_cast<std::int16_t>(y + 1));
    display.drawRectangle(
        static_cast<std::int16_t>(x + 1),
        static_cast<std::int16_t>(y + 2),
        std::max<std::int16_t>(2, static_cast<std::int16_t>(width - 2)),
        std::max<std::int16_t>(1, static_cast<std::int16_t>(height - 1)));
    display.setPixel(x, static_cast<std::int16_t>(y + 2));
    display.setPixel(static_cast<std::int16_t>(x + width - 1), static_cast<std::int16_t>(y + 2));
}
void drawRoad(hal::OledDisplay& display, const std::uint32_t distance) {
    constexpr std::int16_t kStep = 3;
    std::int16_t lastY = kHorizonY;
    std::int16_t lastLeft = roadLeftAt(distance, lastY);
    std::int16_t lastRight = roadRightAt(distance, lastY);
    std::int16_t lastOuterLeft = static_cast<std::int16_t>(lastLeft - 3);
    std::int16_t lastOuterRight = static_cast<std::int16_t>(lastRight + 3);
    for (std::int16_t y = static_cast<std::int16_t>(kHorizonY + kStep); y <= kRoadBottomY;
         y = static_cast<std::int16_t>(y + kStep)) {
        const std::int16_t left = roadLeftAt(distance, y);
        const std::int16_t right = roadRightAt(distance, y);
        const std::int16_t shoulder = static_cast<std::int16_t>(3 + (y - kHorizonY) / 7);
        const std::int16_t outerLeft = static_cast<std::int16_t>(left - shoulder);
        const std::int16_t outerRight = static_cast<std::int16_t>(right + shoulder);
        display.drawLine(lastLeft, lastY, left, y);
        display.drawLine(lastRight, lastY, right, y);
        display.drawLine(lastOuterLeft, lastY, outerLeft, y);
        display.drawLine(lastOuterRight, lastY, outerRight, y);
        lastY = y;
        lastLeft = left;
        lastRight = right;
        lastOuterLeft = outerLeft;
        lastOuterRight = outerRight;
    }
}
void drawMiniTrack(hal::OledDisplay& display, const std::uint32_t distance) {
    constexpr std::int16_t originX = 63;
    constexpr std::int16_t originY = 57;
    for (std::size_t i = 1U; i < kMiniTrack.size(); ++i) {
        display.drawLine(
            static_cast<std::int16_t>(originX + kMiniTrack[i - 1U].x),
            static_cast<std::int16_t>(originY + kMiniTrack[i - 1U].y),
            static_cast<std::int16_t>(originX + kMiniTrack[i].x),
            static_cast<std::int16_t>(originY + kMiniTrack[i].y));
    }
    const std::size_t segment =
        static_cast<std::size_t>((distance / 90U) % (kMiniTrack.size() - 1U));
    const auto marker = kMiniTrack[segment];
    display.fillRectangle(
        static_cast<std::int16_t>(originX + marker.x - 1),
        static_cast<std::int16_t>(originY + marker.y - 1),
        3,
        3);
}
void drawCrashBurst(
    hal::OledDisplay& display,
    const std::int16_t centerX,
    const std::uint32_t phase) {
    const std::int16_t cy = 40;
    const std::int16_t radius = static_cast<std::int16_t>(4 + (phase % 4U) * 2U);
    display.drawLine(
        static_cast<std::int16_t>(centerX - radius),
        cy,
        static_cast<std::int16_t>(centerX + radius),
        cy);
    display.drawLine(
        centerX,
        static_cast<std::int16_t>(cy - radius),
        centerX,
        static_cast<std::int16_t>(cy + radius));
    display.drawLine(
        static_cast<std::int16_t>(centerX - radius + 2),
        static_cast<std::int16_t>(cy - radius + 2),
        static_cast<std::int16_t>(centerX + radius - 2),
        static_cast<std::int16_t>(cy + radius - 2));
    display.drawLine(
        static_cast<std::int16_t>(centerX + radius - 2),
        static_cast<std::int16_t>(cy - radius + 2),
        static_cast<std::int16_t>(centerX - radius + 2),
        static_cast<std::int16_t>(cy + radius - 2));
}
}  // namespace

void Formula1Game::render() {
    display_.clear();
    display_.setFont(hal::DisplayFont::Small);
    display_.setTextColor(hal::PixelColor::White);
    drawCloud(display_, 18, 2);
    drawCloud(display_, 66, 1);
    drawMountains(display_);
    display_.drawHorizontalLine(0, kHorizonY, 128);
    drawRoad(display_, distance_);
    const std::int16_t scroll = static_cast<std::int16_t>((distance_ / 2U) % 13U);
    std::int16_t firstY = static_cast<std::int16_t>(kHorizonY + 3 + (12 - scroll));
    while (firstY >= kHorizonY + 15) {
        firstY = static_cast<std::int16_t>(firstY - 12);
    }
    while (firstY < kHorizonY + 3) {
        firstY = static_cast<std::int16_t>(firstY + 12);
    }
    for (std::int16_t y = firstY; y < kRoadBottomY; y = static_cast<std::int16_t>(y + 12)) {
        const std::int16_t width = roadWidthAt(distance_, y);
        const std::int16_t dashHalf =
            std::max<std::int16_t>(1, static_cast<std::int16_t>(width / 18));
        const std::int16_t dashHeight =
            std::max<std::int16_t>(2, static_cast<std::int16_t>(width / 22));
        const std::int16_t dashY =
            std::max<std::int16_t>(static_cast<std::int16_t>(kHorizonY + 2), y);
        const std::int16_t clippedHeight =
            std::min<std::int16_t>(dashHeight, static_cast<std::int16_t>(kRoadBottomY - dashY));
        if (clippedHeight > 0) {
            display_.fillRectangle(
                static_cast<std::int16_t>(roadCenterAt(distance_, dashY) - dashHalf),
                dashY,
                static_cast<std::int16_t>(dashHalf * 2 + 1),
                clippedHeight);
        }
    }
    for (const Car& car : traffic_) {
        if (car.active) {
            drawTrafficCar(
                display_,
                laneCenterAt(distance_, car.lane, static_cast<std::int16_t>(car.y + 2)),
                car.y);
        }
    }
    if (crashed_) {
        drawCrashBurst(display_, playerX_, (elapsedMs_ + crashStartedAtMs_) / 90U);
    } else {
        drawPlayerCar(display_, playerX_);
    }

    display_.drawHorizontalLine(0, 48, 128);
    if (gameOver_) {
        display_.drawText(34, 20, text::get(text::TextId::GameOver));
        display_.drawText(31, 32, text::get(text::TextId::TapRetry));
    }
    char left1[12]{};
    char left2[12]{};
    char right1[12]{};
    char right2[12]{};
    // Widest texts: time modulo 1000 s, lap and speed at their type maxima, scores modulo 100000.
    static_assert(sizeof(left1) >= sizeof("T999"), "time field");
    static_assert(sizeof(left2) >= sizeof("L255 V65535"), "lap and speed field");
    static_assert(sizeof(right1) >= sizeof("S99999"), "score field");
    static_assert(sizeof(right2) >= sizeof("H99999"), "high-score field");
    text::formatInto(
        left1, sizeof(left1), "T%03lu", static_cast<unsigned long>((elapsedMs_ / 1000U) % 1000U));
    text::formatInto(
        left2,
        sizeof(left2),
        "L%u V%03u",
        static_cast<unsigned>(lap_),
        static_cast<unsigned>(speed_));
    text::formatInto(
        right1, sizeof(right1), "S%05lu", static_cast<unsigned long>(score_ % 100000U));
    text::formatInto(
        right2, sizeof(right2), "H%05lu", static_cast<unsigned long>(highScore_.score % 100000U));
    display_.drawText(1, 50, left1);
    display_.drawText(1, 57, left2);
    display_.drawText(86, 50, right1);
    display_.drawText(86, 57, right2);
    drawMiniTrack(display_, distance_);
    for (std::uint8_t i = 0U; i < crashesRemaining_; ++i) {
        display_.setPixel(static_cast<std::int16_t>(45 + i * 3), 62);
    }
    display_.present();
}

}  // namespace clockfw::game
