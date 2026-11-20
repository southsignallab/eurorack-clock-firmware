/**
 * @file arcade_shell.h
 * @brief Shared retro intro, name-entry, and Top-100 flow for boot Easter eggs.
 * @author Axel Napolitano
 * @copyright 2026 Axel Napolitano
 * @license PolyForm-Noncommercial-1.0.0
 */
#pragma once

#include <array>
#include <cstdint>

#include "game/arcade_leaderboard_store.h"
#include "hal/control_panel.h"
#include "hal/oled_display.h"

namespace clockfw::game {

/** @brief Boot game whose intro art, title, and marquee text the shell shows. */
enum class ArcadeTitle : std::uint8_t {
    PixelRaid,   ///< Pixel Raid fixed shooter.
    Formula1,    ///< Formula 1 racer.
    Breakout,    ///< Brick breaker.
    EggJourney,  ///< Egg Journey lunar traversal.
    Beatknecht   ///< Beatknecht; started with the transport button instead of TAP or encoder.
};

/** @brief Detects the encoder long press that leaves a boot game. */
class LongPressDetector final {
  public:
    /**
     * @brief Feeds one control sample.
     * @param button Encoder push sample.
     * @param nowMs Current monotonic time in milliseconds.
     * @param holdMs Hold time that makes a long press.
     * @param enabled False while a dialog owns the encoder; the press is still tracked.
     * @return True once per press, when it has been held for @p holdMs.
     */
    bool update(
        const hal::ButtonSample& button,
        std::uint32_t nowMs,
        std::uint32_t holdMs,
        bool enabled = true);

    /** @brief Forgets the current press; it can no longer fire. */
    void cancel();

  private:
    /** Time of the last press edge, ms. */
    std::uint32_t pressedAtMs_ = 0U;
    /** Every millisecond value is a valid press time, including 0 (F-074). */
    bool tracking_ = false;
};

/**
 * @brief Shared intro, initials entry, and Top-100 screens around one boot game.
 *
 * The game drives update() and render() from its own loop and takes over controls and rendering
 * while playing() is true. Completing initials entry commits the leaderboard to persistent
 * storage from update().
 */
class ArcadeShell final {
  public:
    /** @brief Screen that currently owns the display and controls. */
    enum class Screen : std::uint8_t {
        Intro,       ///< Title splash and marquee; waits for the start button.
        Playing,     ///< The game owns controls and rendering.
        NameEntry,   ///< Three-initial editor for a qualifying score.
        Leaderboard  ///< Scrollable Top-100 list; RESET restarts.
    };
    /** @brief Request from update() to the game loop. */
    enum class Action : std::uint8_t {
        None,       ///< Nothing to do.
        StartRun,   ///< Leave the intro and start a fresh run.
        RestartRun  ///< Leave the leaderboard and start a fresh run.
    };

    /**
     * @brief Constructs the shared presentation state for one Easter egg.
     * @param display Display used by all shell screens; must outlive the shell.
     * @param title Game whose intro and texts are shown.
     * @param leaderboard Ranking store, or nullptr for a game without a ranking; must outlive the
     *        shell.
     */
    ArcadeShell(hal::OledDisplay& display, ArcadeTitle title, ArcadeLeaderboardStore* leaderboard);
    /**
     * @brief Starts the individual retro intro and refreshes any durable ranking table.
     * @param nowMs Current system time in milliseconds; start of the intro animation.
     */
    void begin(std::uint32_t nowMs);
    /**
     * @brief Advances intro, initials, leaderboard scrolling, and restart controls.
     *
     * Finishing the third initial inserts the score and commits the leaderboard.
     * @param controls Control sample of this loop pass.
     * @param nowMs Current system time in milliseconds; currently unused.
     * @return Action for the game loop.
     */
    Action update(const hal::ControlSample& controls, std::uint32_t nowMs);
    /**
     * @brief Renders whichever non-gameplay arcade screen currently owns the OLED.
     *
     * Draws nothing while playing.
     * @param nowMs Current system time in milliseconds; drives the intro animation.
     */
    void render(std::uint32_t nowMs);
    /**
     * @brief Routes a completed score to initials entry or directly to the Top-100 list.
     *
     * Does nothing without a leaderboard; the shell then stays on Screen::Playing.
     * @param score Final score of the run.
     */
    void finishRun(std::uint32_t score);
    /**
     * @brief Returns true while the game implementation owns controls and rendering.
     * @return true on Screen::Playing.
     */
    bool playing() const;
    /**
     * @brief Returns the current durable rank-one score, or zero when the list is empty.
     * @return Top score of the cached table.
     */
    std::uint32_t highScore() const;
    /**
     * @brief Returns the active shared arcade screen for tests and game integration.
     * @return Current screen.
     */
    Screen screen() const;
#ifdef CLOCK_HOST_TEST
    /** @brief Bypasses the interactive intro in deterministic host gameplay tests. */
    void startImmediatelyForTest();
    /**
     * @brief Places the shell on the leaderboard for deterministic screenshot coverage.
     * @param score Pending score to store.
     * @param highlightedRank Zero-based rank to highlight, or -1 for none.
     */
    void forceLeaderboardForTest(std::uint32_t score, std::int16_t highlightedRank);
#endif

  private:
    /** Leaderboard rows shown at once. */
    static constexpr std::uint8_t kVisibleLeaderboardRows = 5U;
    /** Letters selectable for initials; the encoder wraps around. */
    static constexpr char kInitialAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    /**
     * @brief Renders the game-specific retro splash and horizontally scrolling message.
     * @param nowMs Current system time in milliseconds; the marquee moves 1 pixel per 55 ms.
     */
    void renderIntro(std::uint32_t nowMs);
    /** @brief Renders the shared three-initial Top-100 entry editor. */
    void renderNameEntry();
    /** @brief Renders one scrollable five-row window into the Top-100 table. */
    void renderLeaderboard();
    /**
     * @brief Resolves the localized title for the selected Easter egg.
     * @return Text from ui_text.h; static storage.
     */
    const char* titleText() const;
    /**
     * @brief Resolves the localized marquee sentence for the selected Easter egg.
     * @return Text from ui_text.h; static storage.
     */
    const char* marqueeText() const;
    /**
     * @brief Draws one small-font string horizontally centered on the OLED.
     * @param y Text row, pixels.
     * @param value NUL-terminated text.
     */
    void drawCentered(std::int16_t y, const char* value);
    /**
     * @brief Positions the leaderboard window around a newly inserted rank.
     *
     * The rank appears in the third visible row where the table allows it.
     * @param rank Zero-based rank, or a negative value to scroll to the top.
     */
    void setLeaderboardScrollAround(std::int16_t rank);

    hal::OledDisplay& display_;                             ///< Borrowed display.
    ArcadeTitle title_;                                     ///< Game shown by the shell.
    ArcadeLeaderboardStore* leaderboard_;                   ///< Borrowed store, or nullptr.
    LeaderboardTable table_{};                              ///< Cached copy of the ranking.
    Screen screen_ = Screen::Intro;                         ///< Current screen.
    std::uint32_t introStartedAtMs_ = 0U;                   ///< Intro start time, ms.
    std::uint32_t pendingScore_ = 0U;                       ///< Score awaiting initials.
    std::array<char, 4U> initials_{{'A', 'A', 'A', '\0'}};  ///< Initials being edited.
    std::uint8_t initialPosition_ = 0U;                     ///< Initial being edited, 0..2.
    std::uint8_t alphabetIndex_ = 0U;      ///< Selected letter in kInitialAlphabet.
    std::uint8_t leaderboardScroll_ = 0U;  ///< First visible leaderboard row.
    std::int16_t highlightedRank_ = -1;    ///< Rank to highlight, or -1.
};

}  // namespace clockfw::game
