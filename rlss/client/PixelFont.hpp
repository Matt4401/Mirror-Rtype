/**
 * @file PixelFont.hpp
 * @brief 5 x 7 block font for the menus, drawn with flat squares until the engine can render text.
 */

#pragma once

#include <functional>
#include <string_view>

namespace rlss::client::font {

/** @brief Columns of a glyph. */
inline constexpr int GLYPH_COLUMNS = 5;
/** @brief Rows of a glyph. */
inline constexpr int GLYPH_ROWS = 7;
/** @brief Columns from one character to the next: the glyph and one empty column. */
inline constexpr int ADVANCE = GLYPH_COLUMNS + 1;

/** @brief A horizontal run of lit squares, in squares from the top-left corner of the text. */
struct Run {
    /** @brief First column. */
    int column = 0;
    /** @brief Row. */
    int row = 0;
    /** @brief Number of squares. */
    int length = 0;
};

/**
 * @brief Width of a text, in squares (no trailing empty column).
 * @param text Text; characters without a glyph count as spaces.
 * @return Width, 0 for an empty text.
 */
[[nodiscard]] int width(std::string_view text) noexcept;

/**
 * @brief Calls `draw` for each horizontal run of lit squares of a text: one rectangle each, instead of one per square.
 *
 * Upper-case letters, digits, space and `: - + / . ! ? '` have a glyph; lower-case letters are drawn upper-case,
 * anything else as a space.
 *
 * @param text Text.
 * @param draw Called once per run.
 */
void forEachRun(std::string_view text, const std::function<void(const Run&)>& draw);

}  // namespace rlss::client::font
