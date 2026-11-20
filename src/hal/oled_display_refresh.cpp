/**
 * @file oled_display_refresh.cpp
 * @brief Frame refresh of the OLED framebuffer: dirty-page SPI present and paced I2C refresh.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#include "hal/oled_display.h"

#include <algorithm>

namespace clockfw::hal {

void OledDisplay::present() {
    if (config::kDisplayTransport == config::DisplayTransport::I2c) {
        queueI2cPresent();
        return;
    }
    presentImmediate();
}

bool OledDisplay::service() {
    if (config::kDisplayTransport != config::DisplayTransport::I2c) {
        return false;
    }
    return serviceI2cRefresh();
}

void OledDisplay::presentImmediate() {
    prepareTransportFramebuffer(framebuffer_, queuedI2cFramebuffer_);

    constexpr std::size_t kPageCount = config::kDisplayHeight / 8U;

    bool everyPageSent = true;
    std::size_t page = 0U;
    while (page < kPageCount) {
        const std::size_t pageOffset = page * kPageWidth;
        const bool pageChanged = !hasPresentedFramebuffer_ ||
            !std::equal(
                queuedI2cFramebuffer_.begin() + static_cast<std::ptrdiff_t>(pageOffset),
                queuedI2cFramebuffer_.begin() +
                    static_cast<std::ptrdiff_t>(pageOffset + kPageWidth),
                presentedFramebuffer_.begin() + static_cast<std::ptrdiff_t>(pageOffset));
        if (!pageChanged) {
            ++page;
            continue;
        }

        const std::size_t firstDirtyPage = page;
        std::size_t lastDirtyPage = page;
        while (lastDirtyPage + 1U < kPageCount) {
            const std::size_t nextPage = lastDirtyPage + 1U;
            const std::size_t nextOffset = nextPage * kPageWidth;
            const bool nextPageChanged = !hasPresentedFramebuffer_ ||
                !std::equal(
                    queuedI2cFramebuffer_.begin() + static_cast<std::ptrdiff_t>(nextOffset),
                    queuedI2cFramebuffer_.begin() +
                        static_cast<std::ptrdiff_t>(nextOffset + kPageWidth),
                    presentedFramebuffer_.begin() + static_cast<std::ptrdiff_t>(nextOffset));
            if (!nextPageChanged) {
                break;
            }
            lastDirtyPage = nextPage;
        }

        const std::uint8_t addressWindow[] = {
            0x21U,
            0x00U,
            0x7FU,
            0x22U,
            static_cast<std::uint8_t>(firstDirtyPage),
            static_cast<std::uint8_t>(lastDirtyPage)};
        const std::size_t firstByte = firstDirtyPage * kPageWidth;
        const std::size_t byteCount = (lastDirtyPage - firstDirtyPage + 1U) * kPageWidth;
        const bool windowSent = sendCommands(addressWindow, sizeof(addressWindow));
        const bool dataSent =
            windowSent && sendData(queuedI2cFramebuffer_.data() + firstByte, byteCount);
        if (dataSent) {
            std::copy_n(
                queuedI2cFramebuffer_.begin() + static_cast<std::ptrdiff_t>(firstByte),
                byteCount,
                presentedFramebuffer_.begin() + static_cast<std::ptrdiff_t>(firstByte));
        } else {
            // F-043: the pages keep their old presented copy, so they stay dirty and the
            // next present() sends them again, even if their content does not change.
            everyPageSent = false;
        }
        page = lastDirtyPage + 1U;
    }

    hasPresentedFramebuffer_ = hasPresentedFramebuffer_ || everyPageSent;
}

void OledDisplay::queueI2cPresent() {
    prepareTransportFramebuffer(framebuffer_, queuedI2cFramebuffer_);
    i2cFrameQueued_ = true;
    i2cRefreshPage_ = 0U;
    i2cRefreshOffset_ = 0U;
    i2cRefreshPhase_ = I2cRefreshPhase::SetPageWindow;
}

bool OledDisplay::serviceI2cRefresh() {
    constexpr std::uint8_t kPageCount = static_cast<std::uint8_t>(config::kDisplayHeight / 8U);
    constexpr std::size_t kChunkSize = 24U;

    if (!i2cFrameQueued_) {
        return false;
    }

    if (i2cRefreshPhase_ == I2cRefreshPhase::SetPageWindow) {
        while (i2cRefreshPage_ < kPageCount) {
            const std::size_t pageOffset = static_cast<std::size_t>(i2cRefreshPage_) * kPageWidth;
            const bool pageChanged = !hasPresentedFramebuffer_ ||
                !std::equal(
                    queuedI2cFramebuffer_.begin() + static_cast<std::ptrdiff_t>(pageOffset),
                    queuedI2cFramebuffer_.begin() +
                        static_cast<std::ptrdiff_t>(pageOffset + kPageWidth),
                    presentedFramebuffer_.begin() + static_cast<std::ptrdiff_t>(pageOffset));
            if (pageChanged) {
                break;
            }
            ++i2cRefreshPage_;
        }

        if (i2cRefreshPage_ >= kPageCount) {
            i2cFrameQueued_ = false;
            i2cRefreshPhase_ = I2cRefreshPhase::Idle;
            hasPresentedFramebuffer_ = true;
            return false;
        }

        const std::uint8_t addressWindow[] = {
            0x21U, 0x00U, 0x7FU, 0x22U, i2cRefreshPage_, i2cRefreshPage_};
        if (sendI2cCommandChunk(addressWindow, sizeof(addressWindow))) {
            i2cRefreshOffset_ = 0U;
            i2cRefreshPhase_ = I2cRefreshPhase::SendPageData;
        }
        return true;
    }

    if (i2cRefreshPhase_ == I2cRefreshPhase::SendPageData) {
        const std::size_t pageOffset = static_cast<std::size_t>(i2cRefreshPage_) * kPageWidth;
        const std::size_t remaining = kPageWidth - i2cRefreshOffset_;
        const std::size_t chunkSize = std::min(kChunkSize, remaining);
        const std::size_t byteOffset = pageOffset + i2cRefreshOffset_;
        if (sendI2cDataChunk(queuedI2cFramebuffer_.data() + byteOffset, chunkSize)) {
            std::copy_n(
                queuedI2cFramebuffer_.begin() + static_cast<std::ptrdiff_t>(byteOffset),
                chunkSize,
                presentedFramebuffer_.begin() + static_cast<std::ptrdiff_t>(byteOffset));
            i2cRefreshOffset_ = static_cast<std::uint8_t>(i2cRefreshOffset_ + chunkSize);
            if (i2cRefreshOffset_ >= kPageWidth) {
                ++i2cRefreshPage_;
                i2cRefreshOffset_ = 0U;
                i2cRefreshPhase_ = I2cRefreshPhase::SetPageWindow;
            }
        }
        return true;
    }

    return false;
}

}  // namespace clockfw::hal
