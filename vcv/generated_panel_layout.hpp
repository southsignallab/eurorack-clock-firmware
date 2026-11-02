/**
 * @file generated_panel_layout.hpp
 * @brief Generated VCV Rack front-panel geometry sourced from sim/panel_layout.ini.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 *
 * GENERATED FILE. Run scripts/generate_vcv_panel.py after changing simulator geometry.
 */
#pragma once

#include <array>

namespace clockfw::vcv::panel {

struct PointMm final {
    float x;
    float y;
};

struct RectMm final {
    float x;
    float y;
    float width;
    float height;
};

inline constexpr float kPanelWidthMm = 50.5F;
inline constexpr float kPanelHeightMm = 128.5F;
inline constexpr float kPanelOffsetXmm = 0.15F;
inline constexpr float kPanelOffsetYmm = 0.0967F;
inline constexpr RectMm kDisplay{3.75F, 22.25F, 25.0F, 13.6F};
inline constexpr PointMm kEncoderCenter{40.7F, 29.25F};
inline constexpr float kEncoderDiameterMm = 8.4166F;
inline constexpr PointMm kPlayCenter{8.2F, 52.23F};
inline constexpr PointMm kTapCenter{25.12F, 52.25F};
inline constexpr PointMm kStopCenter{41.75F, 52.35F};
inline constexpr float kButtonActuatorDiameterMm = 9.0F;
inline constexpr PointMm kSyncCenter{7.75F, 70.1F};
inline constexpr PointMm kResetCenter{19.4167F, 70.13F};
inline constexpr float kJackNutDiameterMm = 7.85F;
inline constexpr float kLedDiameterMm = 3.0F;
inline constexpr std::array<PointMm, 8U> kOutputCenters{{
    {7.75F, 88.93F}, {19.4167F, 88.93F}, {31.0833F, 88.93F}, {42.75F, 88.93F}, {7.75F, 107.73F}, {19.4167F, 107.73F}, {31.0833F, 107.73F}, {42.75F, 107.73F}
}};
inline constexpr std::array<PointMm, 8U> kLedCenters{{
    {10.915F, 81.56F}, {22.5817F, 81.56F}, {34.2483F, 81.56F}, {45.915F, 81.56F}, {10.915F, 100.36F}, {22.5817F, 100.36F}, {34.2483F, 100.36F}, {45.915F, 100.36F}
}};
inline constexpr std::array<PointMm, 4U> kScrewCenters{{
    {7.47F, 3.0F}, {43.03F, 3.0F}, {7.47F, 125.5F}, {43.03F, 125.5F}
}};

}  // namespace clockfw::vcv::panel
