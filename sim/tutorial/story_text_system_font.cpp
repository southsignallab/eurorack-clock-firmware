/**
 * @file story_text_system_font.cpp
 * @brief Host system-font backend of the Storybook text renderer (Windows GDI; elsewhere unavailable).
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#include "tutorial/story_text_system_font.h"

#include <stdexcept>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef _WIN32
#include <algorithm>
#include <cstdint>
#include <limits>
#endif

namespace clockfw::sim::tutorial {

#ifdef _WIN32

namespace {

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty())
        return {};
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (required <= 0)
        throw std::runtime_error("Storybook text is not valid UTF-8");
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
            result.data(), required) != required) {
        throw std::runtime_error("cannot convert Storybook text to UTF-16");
    }
    return result;
}

std::wstring familyToWide(const std::string& family) {
    return utf8ToWide(family);
}

int winLongToInt(const LONG value, const char* const context) {
    if (value < static_cast<LONG>(std::numeric_limits<int>::min()) ||
        value > static_cast<LONG>(std::numeric_limits<int>::max())) {
        throw std::runtime_error(
            std::string(context) + " exceeds Storybook renderer integer range");
    }
    return static_cast<int>(value);
}

int paddedPositiveDimension(const LONG value, const int padding, const char* const context) {
    const int base = winLongToInt(value, context);
    if (base > std::numeric_limits<int>::max() - padding) {
        throw std::runtime_error(
            std::string(context) + " exceeds Storybook renderer integer range");
    }
    return std::max(1, base + padding);
}

struct WindowsFontContext {
    HDC dc = nullptr;
    HFONT font = nullptr;
    HGDIOBJ oldFont = nullptr;
    TEXTMETRICW metrics{};

    explicit WindowsFontContext(const StoryFontStyle& style) {
        dc = CreateCompatibleDC(nullptr);
        if (dc == nullptr)
            throw std::runtime_error("cannot create Windows font rendering context");
        const std::wstring family = familyToWide(style.family);
        const int weight = static_cast<int>(std::clamp<std::uint16_t>(style.weight, 100U, 1000U));
        font = CreateFontW(
            -static_cast<int>(style.sizePx), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            family.c_str());
        if (font == nullptr) {
            DeleteDC(dc);
            dc = nullptr;
            throw std::runtime_error("cannot create system Storybook font: " + style.family);
        }
        oldFont = SelectObject(dc, font);
        wchar_t resolved[LF_FACESIZE]{};
        if (GetTextFaceW(dc, LF_FACESIZE, resolved) <= 0 ||
            _wcsicmp(resolved, family.c_str()) != 0) {
            if (oldFont != nullptr)
                SelectObject(dc, oldFont);
            DeleteObject(font);
            DeleteDC(dc);
            dc = nullptr;
            font = nullptr;
            throw std::runtime_error(
                "system Storybook font '" + style.family +
                "' is not installed; install it or choose another family in the Storybook theme");
        }
        if (GetTextMetricsW(dc, &metrics) == FALSE) {
            if (oldFont != nullptr)
                SelectObject(dc, oldFont);
            DeleteObject(font);
            DeleteDC(dc);
            dc = nullptr;
            font = nullptr;
            throw std::runtime_error("cannot read system Storybook font metrics: " + style.family);
        }
    }

    ~WindowsFontContext() {
        if (dc != nullptr && oldFont != nullptr)
            SelectObject(dc, oldFont);
        if (font != nullptr)
            DeleteObject(font);
        if (dc != nullptr)
            DeleteDC(dc);
    }

    WindowsFontContext(const WindowsFontContext&) = delete;
    WindowsFontContext& operator=(const WindowsFontContext&) = delete;
};

}  // namespace

int systemTextWidth(const std::string& text, const StoryFontStyle& font) {
    if (text.empty())
        return 0;
    WindowsFontContext context(font);
    const std::wstring wide = utf8ToWide(text);
    SIZE size{};
    if (GetTextExtentPoint32W(context.dc, wide.data(), static_cast<int>(wide.size()), &size) ==
        FALSE) {
        throw std::runtime_error("cannot measure system Storybook text");
    }
    return winLongToInt(size.cx, "system Storybook text width");
}

int systemFontHeight(const StoryFontStyle& font) {
    WindowsFontContext context(font);
    return std::max(1, static_cast<int>(context.metrics.tmHeight));
}

int drawSystemTextLine(
    TutorialSurface& surface,
    const std::string& text,
    const StoryFontStyle& font,
    const int x,
    const int y,
    const TutorialColor color) {
    if (text.empty())
        return 0;
    WindowsFontContext metricsContext(font);
    const std::wstring wide = utf8ToWide(text);
    SIZE extent{};
    if (GetTextExtentPoint32W(
            metricsContext.dc, wide.data(), static_cast<int>(wide.size()), &extent) == FALSE) {
        throw std::runtime_error("cannot measure system Storybook text");
    }
    const int width = paddedPositiveDimension(extent.cx, 4, "system Storybook glyph width");
    const int height = std::max(1, static_cast<int>(metricsContext.metrics.tmHeight) + 4);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap =
        CreateDIBSection(metricsContext.dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (bitmap == nullptr || pixels == nullptr)
        throw std::runtime_error("cannot create system Storybook glyph raster");
    HGDIOBJ oldBitmap = SelectObject(metricsContext.dc, bitmap);
    std::fill_n(
        static_cast<std::uint8_t*>(pixels),
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0U);
    SetBkColor(metricsContext.dc, RGB(0, 0, 0));
    SetBkMode(metricsContext.dc, OPAQUE);
    SetTextColor(metricsContext.dc, RGB(255, 255, 255));
    if (TextOutW(metricsContext.dc, 2, 1, wide.data(), static_cast<int>(wide.size())) == FALSE) {
        SelectObject(metricsContext.dc, oldBitmap);
        DeleteObject(bitmap);
        throw std::runtime_error("cannot render system Storybook text");
    }

    const auto* bytes = static_cast<const std::uint8_t*>(pixels);
    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            const std::size_t offset =
                (static_cast<std::size_t>(py) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(px)) *
                4U;
            const std::uint8_t coverage =
                std::max({bytes[offset], bytes[offset + 1U], bytes[offset + 2U]});
            if (coverage == 0U)
                continue;
            const std::uint16_t alphaProduct = static_cast<std::uint16_t>(color.alpha) * coverage;
            const TutorialColor blended{
                color.red, color.green, color.blue,
                static_cast<std::uint8_t>((alphaProduct + 127U) / 255U)};
            surface.blendPixel(x + px - 2, y + py - 1, blended);
        }
    }
    SelectObject(metricsContext.dc, oldBitmap);
    DeleteObject(bitmap);
    return winLongToInt(extent.cx, "system Storybook rendered text width");
}

#else

int systemTextWidth(const std::string&, const StoryFontStyle& font) {
    throw std::runtime_error(
        "system Storybook font backend is not available in this host build for '" + font.family +
        "'; use a builtin theme or render on Windows");
}

int systemFontHeight(const StoryFontStyle& font) {
    return systemTextWidth("M", font);
}

int drawSystemTextLine(
    TutorialSurface&,
    const std::string&,
    const StoryFontStyle& font,
    int,
    int,
    TutorialColor) {
    return systemTextWidth("M", font);
}

#endif

}  // namespace clockfw::sim::tutorial
