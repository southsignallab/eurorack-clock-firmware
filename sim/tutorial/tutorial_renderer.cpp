/**
 * @file tutorial_renderer.cpp
 * @brief Implements deterministic full-frame Storybook presentation without desktop capture or UI reconstruction.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "tutorial/tutorial_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "scope_timeline.h"
#include "tutorial/panel_dynamic_layer.h"
#include "tutorial/story_background_image.h"
#include "tutorial/story_text_renderer.h"
#include "tutorial/story_theme.h"
#include "tutorial/tutorial_focus_renderer.h"
#include "tutorial/tutorial_panel_renderer.h"
#include "tutorial/tutorial_scene_renderer.h"
namespace clockfw::sim::tutorial {
namespace {
constexpr int kFrameMargin = 40;
constexpr int kTutorialGap = 36;
constexpr int kScopeHeight = 220;
constexpr int kCardInset = 22;
constexpr int kCardHeader = 48;
int rounded(const float value) {
    return static_cast<int>(std::lround(value));
}
void renderTutorial(
    TutorialSurface& frame,
    SimulatorRuntime& runtime,
    const layout::PanelLayout& panelLayout,
    const PhysicalPresentationState& physicalState,
    scope::Session& scopeSession,
    const StoryTheme& theme,
    const double speedMultiplier) {
    const OutputProfile output{
        static_cast<std::uint16_t>(frame.width()), static_cast<std::uint16_t>(frame.height()), 30U};
    const bool scopeVisible = physicalState.scopeMode == ScopeMode::VisibleChannel;
    const TutorialCompositionLayout composition = resolveTutorialComposition(
        output, scopeVisible, theme.subtitleHeightPx, theme.subtitleSafeBottomPx);
    const ResolvedTutorialFocus focus = resolveTutorialFocus(physicalState, theme);
    const TutorialColor cardFill{
        theme.tutorialSurface.red, theme.tutorialSurface.green, theme.tutorialSurface.blue, 236U};
    const int leftBottom = std::max(
        composition.oledBox.y + composition.oledBox.height,
        composition.scopeBox.y + composition.scopeBox.height);
    const TutorialRect leftCard{
        composition.oledBox.x - kCardInset, composition.oledBox.y - kCardHeader,
        composition.oledBox.width + 2 * kCardInset,
        leftBottom - (composition.oledBox.y - kCardHeader) + kCardInset};
    const TutorialRect rightCard{
        composition.detailBox.x - kCardInset, composition.detailBox.y - kCardHeader,
        composition.detailBox.width + 2 * kCardInset,
        composition.panelBox.y + composition.panelBox.height -
            (composition.detailBox.y - kCardHeader) + kCardInset};
    frame.fillRect(leftCard, cardFill);
    frame.strokeRect(leftCard, theme.tutorialSurfaceBorder, 2);
    frame.fillRect(rightCard, cardFill);
    frame.strokeRect(rightCard, theme.tutorialSurfaceBorder, 2);
    frame.fillRect({leftCard.x, leftCard.y, 6, leftCard.height}, theme.accent);
    frame.fillRect({rightCard.x, rightCard.y, 6, rightCard.height}, theme.accent);
    (void)drawStoryTextLine(
        frame, "CLOCK DISPLAY", theme.monospace, leftCard.x + 24, leftCard.y + 14, theme.muted);
    (void)drawStoryTextLine(
        frame, "CONTROL DETAIL", theme.monospace, rightCard.x + 24, rightCard.y + 14, theme.muted);
    (void)drawStoryTextLine(
        frame, "LOCATION", theme.monospace, rightCard.x + 24, composition.panelBox.y - 30,
        theme.muted);
    renderOled(frame, runtime, composition.oledBox, theme);
    if (scopeVisible)
        renderScope(frame, runtime, scopeSession, composition.scopeBox, theme);

    const TutorialRect fullPanel{
        rounded(panelLayout.panel.x), rounded(panelLayout.panel.y),
        rounded(panelLayout.panel.width), rounded(panelLayout.panel.height)};
    TutorialRect detailSource{
        fullPanel.x, fullPanel.y, fullPanel.width, std::min(fullPanel.height, 420)};
    if (panelFocusPoint(panelLayout, focus.target)) {
        detailSource = panelFocusSourceRect(panelLayout, focus.target);
    }
    TutorialSurface panel = renderPanelBase(panelLayout, runtime);
    const PanelPresentationSnapshot panelSnapshot =
        makePanelPresentationSnapshot(panelLayout, runtime, physicalState, speedMultiplier);
    panel.composite(
        dynamicLayerToSurface(renderPanelDynamicLayer(panelLayout, panelSnapshot)), 0, 0);
    const TutorialRect detailDestination =
        blitPanelRegion(frame, panel, detailSource, composition.detailBox);
    const TutorialRect locatorDestination =
        blitPanelRegion(frame, panel, fullPanel, composition.panelBox);

    if (const auto target =
            panelFocusDestinationRect(panelLayout, focus.target, detailSource, detailDestination)) {
        const int cx = target->x + target->width / 2;
        const int cy = target->y + target->height / 2;
        drawTutorialFocusRing(frame, cx, cy, target->width / 2, theme);
        drawTutorialFocusArrow(
            frame, *target, focus.label.empty() ? focusTargetLabel(focus.target) : focus.label,
            theme, focus.placement);
    }
    if (const auto target =
            panelFocusDestinationRect(panelLayout, focus.target, fullPanel, locatorDestination)) {
        drawTutorialFocusRing(
            frame, target->x + target->width / 2, target->y + target->height / 2, target->width / 2,
            theme);
    }
    drawOledRegionFocus(frame, composition.oledRaster, focus, theme);
}
}  // namespace
TutorialCompositionLayout resolveTutorialComposition(
    const OutputProfile& output,
    const bool scopeVisible,
    const std::uint32_t subtitleHeightPx,
    const std::uint32_t subtitleSafeBottomPx) {
    const int width = static_cast<int>(output.width);
    const int height = static_cast<int>(output.height);
    const int subtitleHeight = static_cast<int>(subtitleHeightPx);
    const int safeBottom = static_cast<int>(subtitleSafeBottomPx);
    const int contentHeight = height - subtitleHeight - safeBottom - 2 * kFrameMargin;
    if (width < 1280 || contentHeight < 420 || subtitleHeight <= 0 || safeBottom < 0) {
        throw std::runtime_error("tutorial output is too small for readable safe-area composition");
    }
    const int contentWidth = width - 2 * kFrameMargin;
    const int leftWidth = (contentWidth * 67) / 100;
    const int rightWidth = contentWidth - leftWidth - kTutorialGap;
    TutorialCompositionLayout layout{};
    layout.content = {kFrameMargin, kFrameMargin, contentWidth, contentHeight};
    const TutorialRect leftCard{kFrameMargin, kFrameMargin, leftWidth, contentHeight};
    const TutorialRect rightCard{
        kFrameMargin + leftWidth + kTutorialGap, kFrameMargin, rightWidth, contentHeight};
    const TutorialRect left{
        leftCard.x + kCardInset, leftCard.y + kCardHeader, leftCard.width - 2 * kCardInset,
        leftCard.height - kCardHeader - kCardInset};
    const TutorialRect right{
        rightCard.x + kCardInset, rightCard.y + kCardHeader, rightCard.width - 2 * kCardInset,
        rightCard.height - kCardHeader - kCardInset};
    const int detailHeight = (right.height * 62) / 100;
    layout.detailBox = {right.x, right.y, right.width, detailHeight};
    layout.panelBox = {
        right.x, right.y + detailHeight + 42, right.width, right.height - detailHeight - 42};
    if (scopeVisible) {
        layout.oledBox = {left.x, left.y, left.width, left.height - kScopeHeight - 20};
        layout.scopeBox = {left.x, left.y + left.height - kScopeHeight, left.width, kScopeHeight};
    } else {
        layout.oledBox = left;
    }
    constexpr int oledWidth = 128;
    constexpr int oledHeight = 64;
    const int scale =
        std::min(layout.oledBox.width / oledWidth, layout.oledBox.height / oledHeight);
    if (scale < 1)
        throw std::runtime_error("OLED area cannot fit integer nearest-neighbour scale");
    const int rasterWidth = oledWidth * scale;
    const int rasterHeight = oledHeight * scale;
    layout.oledRaster = {
        layout.oledBox.x + (layout.oledBox.width - rasterWidth) / 2,
        layout.oledBox.y + (layout.oledBox.height - rasterHeight) / 2, rasterWidth, rasterHeight};
    layout.subtitleStrip = {0, height - safeBottom - subtitleHeight, width, subtitleHeight};
    return layout;
}
namespace {
void paintTutorialBackground(
    TutorialSurface& frame,
    const TutorialColor solid,
    const TutorialSurface* image,
    const StoryBackgroundImageMode mode) {
    frame.clear(solid);
    if (image == nullptr)
        return;
    const TutorialRect source{
        0, 0, static_cast<int>(image->width()), static_cast<int>(image->height())};
    const TutorialRect destination{
        0, 0, static_cast<int>(frame.width()), static_cast<int>(frame.height())};
    if (mode == StoryBackgroundImageMode::Stretch) {
        frame.blitNearest(*image, source, destination);
        return;
    }
    const double scaleX =
        static_cast<double>(destination.width) / static_cast<double>(source.width);
    const double scaleY =
        static_cast<double>(destination.height) / static_cast<double>(source.height);
    if (mode == StoryBackgroundImageMode::Contain) {
        const double scale = std::min(scaleX, scaleY);
        const int width =
            std::max(1, static_cast<int>(std::lround(static_cast<double>(source.width) * scale)));
        const int height =
            std::max(1, static_cast<int>(std::lround(static_cast<double>(source.height) * scale)));
        frame.blitNearest(
            *image, source,
            {(destination.width - width) / 2, (destination.height - height) / 2, width, height});
        return;
    }
    const double scale = std::max(scaleX, scaleY);
    const int cropWidth =
        std::max(1, static_cast<int>(std::floor(static_cast<double>(destination.width) / scale)));
    const int cropHeight =
        std::max(1, static_cast<int>(std::floor(static_cast<double>(destination.height) / scale)));
    const TutorialRect crop{
        (source.width - cropWidth) / 2, (source.height - cropHeight) / 2, cropWidth, cropHeight};
    frame.blitNearest(*image, crop, destination);
}
}  // namespace
TutorialRenderer::TutorialRenderer(
    std::filesystem::path tutorialRoot,
    layout::PanelLayout panelLayout)
    : tutorialRoot_(std::move(tutorialRoot)), panelLayout_(std::move(panelLayout)) {}
const StoryTheme& TutorialRenderer::resolveTheme(const std::string& themeId) {
    if (!cachedTheme_ || cachedThemeId_ != themeId) {
        cachedTheme_ = loadStoryTheme(tutorialRoot_, themeId);
        cachedThemeId_ = themeId;
        cachedBackground_.reset();
        cachedBackgroundPath_.reset();
    }
    return *cachedTheme_;
}
const TutorialSurface* TutorialRenderer::resolveBackgroundImage(const StoryTheme& theme) {
    if (!theme.tutorialBackgroundImage)
        return nullptr;
    if (!cachedBackground_ || !cachedBackgroundPath_ ||
        *cachedBackgroundPath_ != *theme.tutorialBackgroundImage) {
        cachedBackground_ = loadStoryBackgroundImage(*theme.tutorialBackgroundImage);
        cachedBackgroundPath_ = *theme.tutorialBackgroundImage;
    }
    return &*cachedBackground_;
}
TutorialSurface TutorialRenderer::renderScene(
    const Story& story,
    const StoryScene& scene,
    SimulatorRuntime& runtime,
    const PhysicalPresentationState& physicalState,
    const std::string& activeSubtitle,
    const double speedMultiplier) {
    const StoryTheme& theme = resolveTheme(story.theme);
    TutorialSurface frame(story.output.width, story.output.height, theme.background);
    switch (scene.kind) {
        case SceneKind::Chapter:
            renderStoryChapter(frame, scene, theme);
            break;
        case SceneKind::Text:
            renderStoryTextScene(frame, scene, theme);
            break;
        case SceneKind::Callout:
            renderStoryCallout(frame, scene, theme);
            break;
        case SceneKind::Tutorial:
            paintTutorialBackground(
                frame, theme.tutorialBackground, resolveBackgroundImage(theme),
                theme.tutorialBackgroundImageMode);
            renderTutorial(
                frame, runtime, panelLayout_, physicalState, scopeSession_, theme, speedMultiplier);
            break;
    }
    if (scene.kind == SceneKind::Tutorial) {
        const std::string subtitle = !activeSubtitle.empty() ? activeSubtitle : scene.subtitle;
        renderStorySubtitle(frame, subtitle, theme);
    }
    return frame;
}
}  // namespace clockfw::sim::tutorial
