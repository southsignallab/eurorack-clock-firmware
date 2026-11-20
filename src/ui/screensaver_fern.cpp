/**
 * @file screensaver_fern.cpp
 * @brief Barnsley fern screensaver growing within curated crops of the attractor.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "ui/screensaver_renderer.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace clockfw::ui {
namespace {

/** Q12 fixed-point unit used by the fractal renderer. */
constexpr std::int32_t kFixedOne = 4096;

/** Number of animation frames in one complete fern-growth cycle. */
constexpr std::uint32_t kFernCycleFrames = 48U;

/** Iterations hidden before plotting so the attractor has settled. */
constexpr std::uint32_t kFernBurnInIterations = 24U;

/** First visible frame already contains enough structure to be recognizable. */
constexpr std::uint32_t kFernInitialPoints = 96U;

/** Additional Barnsley iterations revealed on every 400-ms screensaver frame. */
constexpr std::uint32_t kFernPointsPerFrame = 28U;

/** One deliberately composed crop of the Barnsley attractor. */
struct FernViewport final {
    std::int32_t minX;
    std::int32_t maxX;
    std::int32_t minY;
    std::int32_t maxY;
};

/**
 * Curated views of the fern. Randomising arbitrary coordinates can yield empty
 * or visually weak OLED frames, so variety is selected only from crops that
 * retain a useful amount of recognisable leaflet structure.
 */
constexpr std::array<FernViewport, 6U> kFernViewports{{
    {-10'240, 11'469, 8'192, 41'370},  // broad crown
    {-7'782, 7'782, 21'299, 41'574},   // upper crown
    {-11'264, 2'867, 15'974, 38'912},  // left leaflets
    {-2'662, 11'674, 15'565, 38'912},  // right leaflets
    {-9'421, 9'421, 11'878, 35'226},   // middle crown
    {-10'650, 10'650, 4'096, 25'805},  // lower branching
}};

/** Deterministic small PRNG used only to select Barnsley affine transforms. */
std::uint32_t nextFernRandom(std::uint32_t& state) {
    state = state * 1'664'525U + 1'013'904'223U;
    return state;
}

/** Maps one Q12 fern point into the deliberately cropped OLED viewport.
 * Coordinates outside the crop deliberately map outside the framebuffer; the
 * OLED primitive clips them, keeping the crop path branch-free.
 */
void mapFernPoint(
    const std::int32_t x,
    const std::int32_t y,
    const FernViewport& viewport,
    std::int16_t& screenX,
    std::int16_t& screenY) {
    const std::int32_t width = viewport.maxX - viewport.minX;
    const std::int32_t height = viewport.maxY - viewport.minY;
    screenX = static_cast<std::int16_t>(
        (static_cast<std::int64_t>(x - viewport.minX) * (hal::OledDisplay::kWidth - 1)) / width);
    screenY = static_cast<std::int16_t>(
        (hal::OledDisplay::kHeight - 1) -
        (static_cast<std::int64_t>(y - viewport.minY) * (hal::OledDisplay::kHeight - 1)) / height);
}

}  // namespace

void ScreensaverRenderer::renderFractal(const std::uint32_t frameIndex) {
    display_.clear();

    // Keep the mathematically deterministic fern, but choose among curated
    // decorative crops at each growth-cycle boundary. Renderer state survives
    // STOP-mode wake/sleep cycles, so a later activation does not always start
    // with the same composition.
    const std::uint32_t cycleFrame = frameIndex % kFernCycleFrames;
    const bool firstFractalFrame = lastFractalFrameIndex_ == kInvalidFrameIndex;
    const bool restarted = !firstFractalFrame && frameIndex < lastFractalFrameIndex_;
    const bool newGrowthCycle = cycleFrame == 0U && frameIndex != lastFractalFrameIndex_;
    if (firstFractalFrame || restarted || newGrowthCycle) {
        selectNextFernViewport();
    }
    lastFractalFrameIndex_ = frameIndex;
    const FernViewport& viewport = kFernViewports[fernViewportIndex_];
    const std::uint32_t visiblePoints = kFernInitialPoints + cycleFrame * kFernPointsPerFrame;
    const std::uint32_t totalIterations = kFernBurnInIterations + visiblePoints;

    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t randomState = 0x6D2B79F5U;

    for (std::uint32_t iteration = 0U; iteration < totalIterations; ++iteration) {
        const std::uint32_t selector = nextFernRandom(randomState) % 100U;
        const std::int32_t previousX = x;
        const std::int32_t previousY = y;

        if (selector < 1U) {
            // Stem: x' = 0, y' = 0.16y.
            x = 0;
            y = static_cast<std::int32_t>((655LL * previousY) / kFixedOne);
        } else if (selector < 86U) {
            // Dominant leaflet transform.
            x = static_cast<std::int32_t>((3'482LL * previousX + 164LL * previousY) / kFixedOne);
            y = static_cast<std::int32_t>((-164LL * previousX + 3'482LL * previousY) / kFixedOne) +
                6'554;
        } else if (selector < 93U) {
            // Left leaflet transform.
            x = static_cast<std::int32_t>((819LL * previousX - 1'065LL * previousY) / kFixedOne);
            y = static_cast<std::int32_t>((942LL * previousX + 901LL * previousY) / kFixedOne) +
                6'554;
        } else {
            // Right leaflet transform.
            x = static_cast<std::int32_t>((-614LL * previousX + 1'147LL * previousY) / kFixedOne);
            y = static_cast<std::int32_t>((1'065LL * previousX + 983LL * previousY) / kFixedOne) +
                1'802;
        }

        if (iteration >= kFernBurnInIterations) {
            std::int16_t screenX = 0;
            std::int16_t screenY = 0;
            mapFernPoint(x, y, viewport, screenX, screenY);
            display_.setPixel(screenX, screenY);
        }
    }

    display_.present();
}

void ScreensaverRenderer::selectNextFernViewport() {
    const std::uint8_t previousIndex = fernViewportIndex_;
    fernViewportRandomState_ = nextFernRandom(fernViewportRandomState_);
    std::uint8_t candidate = static_cast<std::uint8_t>(
        fernViewportRandomState_ % static_cast<std::uint32_t>(kFernViewports.size()));
    if (candidate == previousIndex) {
        candidate = static_cast<std::uint8_t>(
            (static_cast<std::size_t>(candidate) + 1U) % kFernViewports.size());
    }
    fernViewportIndex_ = candidate;
}
}  // namespace clockfw::ui
