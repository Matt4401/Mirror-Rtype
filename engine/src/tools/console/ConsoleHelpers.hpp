/**
 * @file ConsoleHelpers.hpp
 * @brief Internal helpers of the Console: safe output and tab-completion utilities.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tools::detail {

/**
 * @brief Makes untrusted text safe to show: control characters become `?`, text past `MAX_LINE_LENGTH` is cut.
 * @param text Text.
 * @return The safe copy, ending with `...` if it was cut.
 */
[[nodiscard]] std::string printable(std::string_view text);

/**
 * @brief Tells whether a line holds only spaces and tabs.
 * @param line Line.
 * @return `true` if the line is empty or blank.
 */
[[nodiscard]] bool isBlankLine(std::string_view line) noexcept;

/**
 * @brief Finds the longest prefix shared by every word.
 * @param words Words.
 * @return The shared prefix; empty if there is no word.
 */
[[nodiscard]] std::string commonPrefix(const std::vector<std::string>& words);

/** @brief Position of the word being typed at the end of a line. */
struct WordCursor {
    /** @brief Index of the first character of the word being typed. */
    std::size_t start = 0;
    /** @brief Index of that word in its statement (0 for the name). */
    std::size_t index = 0;
    /** @brief First word of the statement, if the cursor is past it. */
    std::string name;
};

/**
 * @brief Finds the word being typed at the end of a line.
 * @param line Line being typed.
 * @return Nothing if the line is invalid (open quote...) or the word holds a quote or a comment: no completion then.
 */
[[nodiscard]] std::optional<WordCursor> locateLastWord(std::string_view line);

}  // namespace tools::detail
