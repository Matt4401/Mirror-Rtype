/**
 * @file DevConsoleTypes.hpp
 * @brief Plain types of the in-game developer console: editing actions, key bindings and settings.
 */

#pragma once

#include <cstdint>
#include <string>

#include "components/Renderable.hpp"
#include "core/services/InputCodes.hpp"

namespace tools {

/** @brief Editing action of the developer console, triggered by a key. */
enum class EditAction : std::uint8_t {
    /** @brief Runs the line. */
    Submit,
    /** @brief Completes the word being typed. */
    Complete,
    /** @brief Erases the character before the cursor. */
    Backspace,
    /** @brief Erases the character after the cursor. */
    Delete,
    /** @brief Moves the cursor one character left. */
    Left,
    /** @brief Moves the cursor one character right. */
    Right,
    /** @brief Moves the cursor to the start of the line. */
    Home,
    /** @brief Moves the cursor to the end of the line. */
    End,
    /** @brief Recalls the previous line of the history. */
    HistoryPrevious,
    /** @brief Recalls the next line of the history, then the line being typed. */
    HistoryNext,
    /** @brief Scrolls the output one page up (older lines). */
    PageUp,
    /** @brief Scrolls the output one page down (newer lines). */
    PageDown,
    /** @brief Closes the console. */
    Close,
};

/** @brief Key triggering an editing action. */
struct KeyBinding {
    /** @brief Engine key. */
    engine::Key key;
    /** @brief Action it triggers. */
    EditAction action;
    /** @brief Whether holding the key repeats the action. */
    bool repeats;
};

/**
 * @brief Settings of the developer console window.
 */
struct DevConsoleConfig {
    /** @brief Key opening and closing the console: the key left of `1`, `~` on QWERTY and `²` on AZERTY. */
    engine::Key toggleKey = engine::Key::Grave;
    /** @brief Height of the console, as a fraction of the window height. */
    float heightRatio = 0.5F;
    /** @brief Font size in pixels. */
    float fontSize = 18.0F;
    /** @brief Space between characters, in pixels. */
    float letterSpacing = 1.0F;
    /** @brief Extra space between lines, in pixels. */
    float lineSpacing = 4.0F;
    /** @brief Margin around the text, in pixels. */
    float padding = 8.0F;
    /** @brief Font file; empty for the default font. */
    std::string fontPath;
    /** @brief Seconds a key is held before it repeats. */
    float repeatDelay = 0.4F;
    /** @brief Seconds between two repeats. */
    float repeatInterval = 0.04F;
    /** @brief Seconds between two blinks of the cursor. */
    float caretBlink = 0.5F;
    /** @brief Background of the window. */
    ecs::Rgba background = {.r = 12, .g = 14, .b = 20, .a = 225};
    /** @brief Line under the window and above the input line. */
    ecs::Rgba border = {.r = 90, .g = 110, .b = 140, .a = 255};
    /** @brief Normal output and typed text. */
    ecs::Rgba textColor = {.r = 225, .g = 225, .b = 225, .a = 255};
    /** @brief Echoed input lines. */
    ecs::Rgba echoColor = {.r = 130, .g = 170, .b = 230, .a = 255};
    /** @brief Error lines. */
    ecs::Rgba errorColor = {.r = 240, .g = 100, .b = 90, .a = 255};
    /** @brief Scroll indicator. */
    ecs::Rgba hintColor = {.r = 150, .g = 150, .b = 150, .a = 255};
};

}  // namespace tools
