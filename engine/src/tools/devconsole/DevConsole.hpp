/**
 * @file DevConsole.hpp
 * @brief State and editing logic of the in-game developer console, independent of any window.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "tools/devconsole/DevConsoleTypes.hpp"
#include "tools/devconsole/InputLine.hpp"

namespace tools {

class Console;

/**
 * @brief State of the in-game developer console: open or closed, input line, history browsing and output scroll.
 *
 * Turns typed characters and editing actions into console operations: `Submit` runs the line through the `Console`,
 * `Complete` applies its tab completion (listing the candidates when several are left), the history actions recall
 * executed lines (the line being typed comes back after the newest one). Input is ignored while closed.
 *
 * Draws nothing and reads no device: `DevConsolePlugin` feeds it, `DevConsoleView` draws it.
 */
class DevConsole {
  public:
    /**
     * @brief Creates a closed console.
     * @param console Interpreter running the lines; must outlive this object.
     */
    explicit DevConsole(Console& console);

    /** @brief Tells whether the console is open. @return `true` while it takes the input. */
    [[nodiscard]] bool isOpen() const noexcept { return _open; }

    /** @brief Opens the console. */
    void open() noexcept { _open = true; }

    /** @brief Closes the console; the line being typed is kept for the next opening. */
    void close() noexcept { _open = false; }

    /** @brief Opens a closed console, closes an open one. */
    void toggle() noexcept { _open = !_open; }

    /**
     * @brief Types a character at the cursor (ignored while closed).
     * @param cp Unicode code point.
     */
    void type(char32_t cp);

    /**
     * @brief Runs an editing action (ignored while closed).
     * @param action Action.
     */
    void apply(EditAction action);

    /**
     * @brief Scrolls the output.
     * @param lines Lines to scroll; positive shows older lines, negative newer ones.
     */
    void scroll(int lines) noexcept;

    /**
     * @brief Sets how many output lines the view shows, used for page scrolling and to bound the scroll.
     * @param lines Visible output lines.
     */
    void setPageSize(std::size_t lines) noexcept;

    /** @brief Gets the input line. @return Line being typed. */
    [[nodiscard]] const InputLine& input() const noexcept { return _input; }

    /** @brief Gets the scroll. @return Output lines hidden below the view (0 shows the newest lines). */
    [[nodiscard]] std::size_t scrollOffset() const noexcept { return _scroll; }

    /** @brief Gets the interpreter. @return The console running the lines. */
    [[nodiscard]] const Console& console() const noexcept { return *_console; }

  private:
    /** @brief Runs the line and resets the input, the history browsing and the scroll. */
    void submit();

    /** @brief Applies the tab completion of the line, printing the candidates when several are left. */
    void complete();

    /**
     * @brief Moves in the history.
     * @param older `true` for an older line, `false` for a newer one.
     */
    void browseHistory(bool older);

    /** @brief Largest scroll that still fills the view. */
    [[nodiscard]] std::size_t maxScroll() const noexcept;

    /** @brief Interpreter, never null. */
    Console* _console;
    /** @brief Line being typed. */
    InputLine _input;
    /** @brief Whether the console takes the input. */
    bool _open = false;
    /** @brief Output lines hidden below the view. */
    std::size_t _scroll = 0;
    /** @brief Visible output lines. */
    std::size_t _pageSize = 1;
    /** @brief History entry shown, or nothing while editing a new line. */
    std::optional<std::size_t> _historyIndex;
    /** @brief Line being typed before browsing the history, restored after the newest entry. */
    std::string _draft;
};

}  // namespace tools
