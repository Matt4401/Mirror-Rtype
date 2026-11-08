/**
 * @file UiTextInput.hpp
 * @brief Single-line text field component.
 */

#pragma once

#include <cstddef>
#include <string>

#include "components/Renderable.hpp"
#include "ui/components/FontId.hpp"

namespace ui {

/**
 * @brief Single-line text field, edited while it has the focus.
 *
 * Typed characters are appended (UTF-8), Backspace erases the last character and repeats while held, Enter publishes
 * a `UiTextSubmittedEvent`. Add `UiFocusable` so it can get the focus; clicking it also focuses it.
 */
struct UiTextInput {
    /** @brief Default width in pixels. */
    static constexpr float DEFAULT_WIDTH = 240.0F;
    /** @brief Default height in pixels. */
    static constexpr float DEFAULT_HEIGHT = 36.0F;
    /** @brief Default font size in pixels. */
    static constexpr float DEFAULT_FONT_SIZE = 20.0F;
    /** @brief Default maximum length, in characters. */
    static constexpr std::size_t DEFAULT_MAX_LENGTH = 32;

    /** @brief Current text, UTF-8. */
    std::string text;
    /** @brief Text shown, dimmed, while `text` is empty. */
    std::string placeholder;
    /** @brief Action name sent with `UiTextSubmittedEvent`. */
    std::string action;
    /** @brief If not empty, the only ASCII characters accepted (e.g. "0123456789."); non-ASCII is then refused. */
    std::string allowed;
    /** @brief Maximum number of characters (code points). */
    std::size_t maxLength = DEFAULT_MAX_LENGTH;
    /** @brief Width in pixels. */
    float width = DEFAULT_WIDTH;
    /** @brief Height in pixels. */
    float height = DEFAULT_HEIGHT;
    /** @brief Font size in pixels. */
    float fontSize = DEFAULT_FONT_SIZE;
    /** @brief Font of the text. */
    FontId fontId = NO_FONT;
    /** @brief Background colour. */
    ecs::Rgba background = {.r = 30, .g = 30, .b = 30, .a = 230};
    /** @brief Border colour. */
    ecs::Rgba border = {.r = 140, .g = 140, .b = 140, .a = 255};
    /** @brief Text and cursor colour. */
    ecs::Rgba textColor = {.r = 255, .g = 255, .b = 255, .a = 255};
    /** @brief Placeholder colour. */
    ecs::Rgba placeholderColor = {.r = 150, .g = 150, .b = 150, .a = 255};
};

}  // namespace ui
