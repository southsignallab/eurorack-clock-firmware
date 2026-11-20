/**
 * @file performance_renderer_icons.cpp
 * @brief Small status glyphs of the performance screen: lock and humanize icons, channel-mode letters.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "ui/performance_renderer.h"

namespace clockfw::ui {

void PerformanceRenderer::drawLockIcon(const std::int16_t x, const std::int16_t y) {
    display_.drawRectangle(x, static_cast<std::int16_t>(y + 3), 5, 4);
    display_.drawHorizontalLine(static_cast<std::int16_t>(x + 1), y, 3);
    display_.drawVerticalLine(x, static_cast<std::int16_t>(y + 1), 3);
    display_.drawVerticalLine(
        static_cast<std::int16_t>(x + 4), static_cast<std::int16_t>(y + 1), 3);
}

void PerformanceRenderer::drawHumanizeIcon(const std::int16_t x, const std::int16_t y) {
    // Compact stick figure made only from public OLED primitives.
    display_.drawRectangle(static_cast<std::int16_t>(x + 2), y, 3, 3);
    display_.drawVerticalLine(
        static_cast<std::int16_t>(x + 3), static_cast<std::int16_t>(y + 3), 3);
    display_.drawHorizontalLine(x, static_cast<std::int16_t>(y + 4), 7);
    display_.drawLine(
        static_cast<std::int16_t>(x + 3),
        static_cast<std::int16_t>(y + 5),
        static_cast<std::int16_t>(x + 1),
        static_cast<std::int16_t>(y + 7));
    display_.drawLine(
        static_cast<std::int16_t>(x + 3),
        static_cast<std::int16_t>(y + 5),
        static_cast<std::int16_t>(x + 5),
        static_cast<std::int16_t>(y + 7));
}

char PerformanceRenderer::statusModeCharacter(const ChannelMode mode) {
    switch (mode) {
        case ChannelMode::Off:
            return 'O';
        case ChannelMode::Euclid:
            return 'E';
        case ChannelMode::Sequencer:
            return 'S';
        case ChannelMode::Clock:
        default:
            return 'C';
    }
}

}  // namespace clockfw::ui
