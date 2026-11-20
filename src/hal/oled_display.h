/**
 * @file oled_display.h
 * @brief Dependency-free framebuffer and SSD1306/SSD1315 transport abstraction.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */

#pragma once

#include <array>
#include <cstdint>

#include "config.h"

namespace clockfw::hal {

/** @brief Font roles supported by the display abstraction. */
enum class DisplayFont : std::uint8_t { Small, Tempo, TempoLarge };

/** @brief Monochrome pixel color used by drawing operations. */
enum class PixelColor : std::uint8_t { Black, White };

/** @brief Bounding rectangle returned by text measurement. */
struct TextBounds {
    /** Left edge in pixels; the cursor x passed to measureText(). */
    std::int16_t x = 0;
    /** Top edge in pixels; the cursor y passed to measureText(). */
    std::int16_t y = 0;
    /** Text width in pixels; 0 for an empty or null string. */
    std::uint16_t width = 0U;
    /** Glyph height of the selected font in pixels. */
    std::uint16_t height = 0U;
};

/**
 * @brief Owns the 1 KiB framebuffer and the minimal SSD1306/SSD1315 protocol implementation.
 *
 * The class deliberately exposes only primitives required by the firmware UI.
 * It has no dependency on Adafruit GFX or Adafruit SSD1306. Bus selection is a
 * compile-time configuration choice in config.h and is invisible to callers.
 */
class OledDisplay final {
  public:
    /** @brief Physical display width in pixels. */
    static constexpr std::int16_t kWidth = static_cast<std::int16_t>(config::kDisplayWidth);

    /** @brief Physical display height in pixels. */
    static constexpr std::int16_t kHeight = static_cast<std::int16_t>(config::kDisplayHeight);

    /** @brief Number of bytes in the page-oriented monochrome framebuffer. */
    static constexpr std::size_t kFramebufferSize =
        static_cast<std::size_t>(config::kDisplayWidth) * config::kDisplayHeight / 8U;

    /**
     * @brief Initializes the configured bus and OLED controller.
     * @return False when the transport could not be set up or, on I2C, no display answered;
     * true after the controller setup and a blank first frame.
     *
     * Blocking: includes the reset delays and finishes the first I2C frame synchronously.
     * Setup commands are sent without a retry path.
     */
    [[nodiscard]] bool begin();

    /** @brief Clears the in-memory framebuffer. */
    void clear();

    /**
     * @brief Publishes the current framebuffer for display; SPI transfers immediately, I2C
     * is deferred.
     *
     * SPI sends only the pages that differ from the last transferred image; pages whose
     * transfer failed stay dirty. I2C queues the frame for service() and restarts any
     * refresh in progress.
     */
    void present();

    /**
     * @brief Services at most one deferred I2C bus transaction.
     * @return true when one I2C transaction was attempted, false when no deferred work exists.
     *
     * SPI builds keep their immediate dirty-page transfer path, so this method
     * becomes a cheap no-op. Callers may therefore service it unconditionally.
     */
    [[nodiscard]] bool service();

    /**
     * @brief Selects the font role used by subsequent text operations.
     * @param font Font for measureText(), drawText() and drawCharacter().
     */
    void setFont(DisplayFont font);

    /**
     * @brief Selects the monochrome text color.
     * @param color Color of set glyph pixels; unset glyph pixels are left unchanged.
     */
    void setTextColor(PixelColor color);

    /**
     * @brief Measures a null-terminated text string using the selected font.
     * @param text String to measure; nullptr measures as empty.
     * @param cursorX Left edge in pixels, copied into the result.
     * @param cursorY Top edge in pixels, copied into the result.
     * @return Bounds in pixels. The small font advances 6 pixels per character (5 pixels
     * wide, 7 high); tempo fonts add the glyph widths and the glyph spacing.
     */
    TextBounds measureText(const char* text, std::int16_t cursorX, std::int16_t cursorY) const;

    /**
     * @brief Draws text at the specified cursor position using the selected font.
     * @param x Left edge of the first glyph in pixels.
     * @param y Top edge of the glyphs in pixels.
     * @param text Null-terminated string; nullptr draws nothing.
     *
     * Pixels outside the panel are clipped.
     */
    void drawText(std::int16_t x, std::int16_t y, const char* text);

    /**
     * @brief Draws one ASCII character at the specified cursor position.
     * @param x Left edge of the glyph in pixels.
     * @param y Top edge of the glyph in pixels.
     * @param character Character to draw with the selected font.
     */
    void drawCharacter(std::int16_t x, std::int16_t y, char character);

    /**
     * @brief Draws a one-pixel horizontal line.
     * @param x Left end in pixels.
     * @param y Row in pixels.
     * @param width Length in pixels; 0 or less draws nothing.
     * @param color Pixel color.
     */
    void drawHorizontalLine(
        std::int16_t x,
        std::int16_t y,
        std::int16_t width,
        PixelColor color = PixelColor::White);

    /**
     * @brief Draws a one-pixel vertical line.
     * @param x Column in pixels.
     * @param y Top end in pixels.
     * @param height Length in pixels; 0 or less draws nothing.
     * @param color Pixel color.
     */
    void drawVerticalLine(
        std::int16_t x,
        std::int16_t y,
        std::int16_t height,
        PixelColor color = PixelColor::White);

    /**
     * @brief Draws a line between two pixel coordinates, both end points included.
     * @param x0 Start column in pixels.
     * @param y0 Start row in pixels.
     * @param x1 End column in pixels.
     * @param y1 End row in pixels.
     * @param color Pixel color.
     */
    void drawLine(
        std::int16_t x0,
        std::int16_t y0,
        std::int16_t x1,
        std::int16_t y1,
        PixelColor color = PixelColor::White);

    /**
     * @brief Draws a rectangle outline.
     * @param x Left edge in pixels.
     * @param y Top edge in pixels.
     * @param width Width in pixels; 0 or less draws nothing.
     * @param height Height in pixels; 0 or less draws nothing.
     * @param color Pixel color.
     */
    void drawRectangle(
        std::int16_t x,
        std::int16_t y,
        std::int16_t width,
        std::int16_t height,
        PixelColor color = PixelColor::White);

    /**
     * @brief Draws a filled rectangle.
     * @param x Left edge in pixels.
     * @param y Top edge in pixels.
     * @param width Width in pixels; 0 or less draws nothing.
     * @param height Height in pixels; 0 or less draws nothing.
     * @param color Pixel color.
     */
    void fillRectangle(
        std::int16_t x,
        std::int16_t y,
        std::int16_t width,
        std::int16_t height,
        PixelColor color = PixelColor::White);

    /**
     * @brief Sets one framebuffer pixel with clipping.
     * @param x Column in pixels; outside 0..kWidth-1 is ignored.
     * @param y Row in pixels; outside 0..kHeight-1 is ignored.
     * @param color Pixel color.
     */
    void setPixel(std::int16_t x, std::int16_t y, PixelColor color = PixelColor::White);

    /**
     * @brief Changes the OLED panel contrast without modifying framebuffer contents.
     * @param contrast Controller contrast value 0..255 (command 0x81); sent immediately.
     */
    void setContrast(std::uint8_t contrast);

    /**
     * @brief Switches the OLED panel on or off while preserving display RAM.
     * @param enabled True sends display-on (0xAF), false display-off (0xAE); sent
     * immediately.
     */
    void setPower(bool enabled);

    /**
     * @brief Selects 0-degree or deterministic software-rotated 180-degree panel
     * orientation.
     * @param rotated True rotates the transferred image by 180 degrees. A change forces a
     * complete refresh on the next present(); the same value again does nothing.
     */
    void setRotation180(bool rotated);

#if defined(CLOCK_HOST_TEST) || defined(CLOCK_SIMULATOR)
    /** @brief Returns the raw canonical drawing framebuffer used by firmware renderers. */
    const std::array<std::uint8_t, kFramebufferSize>& framebufferForTest() const;

    /** @brief Returns the bytes last confirmed as transferred to the physical-display model. */
    const std::array<std::uint8_t, kFramebufferSize>& presentedFramebufferForTest() const;
#endif

#if defined(CLOCK_SIMULATOR)
    /**
     * @brief Returns the panel image after applying the same transfer rotation used on hardware.
     *
     * The simulator models the bytes that would be written into OLED display RAM while the
     * controller remains in CLOCK's canonical A1/C8 scan orientation.
     */
    const std::array<std::uint8_t, kFramebufferSize>& panelFramebufferForSimulator() const;
#endif

  private:
    /** Bytes per framebuffer page: one byte per display column. */
    static constexpr std::size_t kPageWidth = static_cast<std::size_t>(config::kDisplayWidth);

    /**
     * @brief Configures the I2C transport and resolves the OLED address.
     * @return True when the configured address, or with address 0 either 0x3C or 0x3D,
     * acknowledged; false otherwise or when the peripheral setup failed.
     */
    bool beginI2c();

    /**
     * @brief Configures the SPI transport GPIO and peripheral.
     * @return False when a pin has no supported port or the peripheral setup failed.
     */
    bool beginSpi();

    /** @brief Pulses the optional active-low hardware reset line when one is configured. */
    void resetController();

    /**
     * @brief Sends one controller command byte over the selected transport.
     * @param command Controller command byte; a failed transfer is not retried.
     */
    void sendCommand(std::uint8_t command);

    /**
     * @brief Sends a contiguous controller command sequence in one bus transaction; false on
     * a failed transfer.
     * @param commands First command byte; not retained.
     * @param size Number of command bytes; on I2C at most 31.
     * @return True when the transfer completed; false for nullptr, size 0 or a failure.
     */
    [[nodiscard]] bool sendCommands(const std::uint8_t* commands, std::size_t size);

    /**
     * @brief Sends a contiguous block of framebuffer data; false when any transfer failed.
     * @param data First data byte; not retained.
     * @param size Number of bytes; I2C splits them into transactions of at most 24 bytes.
     * @return True when every transfer completed.
     */
    [[nodiscard]] bool sendData(const std::uint8_t* data, std::size_t size);

    /** @brief Runs the existing immediate dirty-page transfer used by SPI builds. */
    void presentImmediate();

    /** @brief Queues the newest complete framebuffer as the current I2C refresh target. */
    void queueI2cPresent();

    /**
     * @brief Services one I2C address-window or data transaction.
     * @return True when a transaction was attempted (a failed one is repeated on the next
     * call); false when no frame is queued or the queued frame is complete.
     *
     * Unchanged pages are skipped. A data transaction carries at most 24 bytes.
     */
    bool serviceI2cRefresh();

    /**
     * @brief Sends one bounded I2C data transaction and reports ACK success.
     * @param data First data byte; not retained.
     * @param size Number of bytes, 1..31.
     * @return True on success; false for nullptr, size 0, a size above 31 or a failed
     * transfer.
     */
    bool sendI2cDataChunk(const std::uint8_t* data, std::size_t size);

    /**
     * @brief Sends one bounded I2C command transaction and reports ACK success.
     * @param commands First command byte; not retained.
     * @param size Number of bytes, 1..31.
     * @return True on success; false for nullptr, size 0, a size above 31 or a failed
     * transfer.
     */
    bool sendI2cCommandChunk(const std::uint8_t* commands, std::size_t size);

    /**
     * @brief Probes one I2C address for an ACK response.
     * @param address 7-bit I2C address.
     * @return True when the device acknowledged.
     */
    bool isI2cDevicePresent(std::uint8_t address);

    /**
     * @brief Writes one framebuffer pixel with clipping.
     * @param x Column in pixels; outside 0..kWidth-1 is ignored.
     * @param y Row in pixels; outside 0..kHeight-1 is ignored.
     * @param color Pixel color.
     */
    void drawPixel(std::int16_t x, std::int16_t y, PixelColor color);

    /**
     * @brief Copies the canonical framebuffer into the physical transfer orientation.
     * @param source Canonical framebuffer as drawn by the renderers.
     * @param destination Receives the bytes to transfer: a copy, or with rotation180_ the
     * image rotated by 180 degrees.
     */
    void prepareTransportFramebuffer(
        const std::array<std::uint8_t, kFramebufferSize>& source,
        std::array<std::uint8_t, kFramebufferSize>& destination) const;

    /**
     * @brief Draws one project-owned compact 5x7 glyph at its native resolution.
     * @param x Left edge in pixels.
     * @param y Top edge in pixels.
     * @param character Character; see font::glyphFor().
     */
    void drawSmallGlyph(std::int16_t x, std::int16_t y, char character);

    /**
     * @brief Draws one dedicated proportional tempo numeral at its native resolution.
     * @param x Left edge in pixels.
     * @param y Top edge in pixels.
     * @param character Character; see font::tempoGlyphFor().
     */
    void drawTempoGlyph(std::int16_t x, std::int16_t y, char character);

    /**
     * @brief Draws one larger CLOCK-mode tempo numeral at its native resolution.
     * @param x Left edge in pixels.
     * @param y Top edge in pixels.
     * @param character Character; see font::largeTempoGlyphFor().
     */
    void drawLargeTempoGlyph(std::int16_t x, std::int16_t y, char character);

    /**
     * @brief Measures a complete string rendered with the dedicated tempo numeral font.
     * @param text Null-terminated string; nullptr or empty measures 0.
     * @return Sum of the glyph widths plus the spacing between glyphs, in pixels.
     */
    std::uint16_t measureTempoTextWidth(const char* text) const;

    /**
     * @brief Measures a complete string rendered with the larger CLOCK-mode tempo font.
     * @param text Null-terminated string; nullptr or empty measures 0.
     * @return Sum of the glyph widths plus the spacing between glyphs, in pixels.
     */
    std::uint16_t measureLargeTempoTextWidth(const char* text) const;

    /** @brief Step of the incremental I2C refresh serviced by serviceI2cRefresh(). */
    enum class I2cRefreshPhase : std::uint8_t { Idle, SetPageWindow, SendPageData };

    /** Canonical framebuffer that the drawing operations write. */
    std::array<std::uint8_t, kFramebufferSize> framebuffer_{};
    /** Bytes confirmed as transferred to the controller, in transfer orientation. */
    std::array<std::uint8_t, kFramebufferSize> presentedFramebuffer_{};
    /** Frame in transfer orientation being sent; used by the SPI and the I2C path. */
    std::array<std::uint8_t, kFramebufferSize> queuedI2cFramebuffer_{};
#if defined(CLOCK_SIMULATOR)
    mutable std::array<std::uint8_t, kFramebufferSize> simulatorPanelFramebuffer_{};
#endif
    /** Font selected by setFont(). */
    DisplayFont font_ = DisplayFont::Small;
    /** Glyph color selected by setTextColor(). */
    PixelColor textColor_ = PixelColor::White;
    /** 7-bit I2C address resolved by beginI2c(). */
    std::uint8_t i2cAddress_ = 0U;
    /** Page the I2C refresh is working on. */
    std::uint8_t i2cRefreshPage_ = 0U;
    /** Next byte of the current page to send over I2C. */
    std::uint8_t i2cRefreshOffset_ = 0U;
    /** Current step of the I2C refresh. */
    I2cRefreshPhase i2cRefreshPhase_ = I2cRefreshPhase::Idle;
    /** True while an I2C frame waits to be completed by service(). */
    bool i2cFrameQueued_ = false;
    /** False until one complete frame was transferred; until then every page is dirty. */
    bool hasPresentedFramebuffer_ = false;
    /** True when the transferred image is rotated by 180 degrees. */
    bool rotation180_ = false;
};

}  // namespace clockfw::hal
