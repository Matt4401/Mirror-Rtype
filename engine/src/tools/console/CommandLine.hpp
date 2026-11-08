/**
 * @file CommandLine.hpp
 * @brief Splits a console line into statements and words.
 */

#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace tools {

/** @brief Words of one statement: the command or cvar name, then its arguments. */
using Statement = std::vector<std::string>;

/** @brief Longest accepted console line, in bytes. */
inline constexpr std::size_t MAX_LINE_LENGTH = 1024;
/** @brief Most words accepted on one line, all statements together. */
inline constexpr std::size_t MAX_LINE_WORDS = 64;

/**
 * @brief Splits a console line into statements.
 *
 * - Words are separated by spaces or tabs; `;` ends a statement (`set a 1; set b 2`).
 * - Double quotes group a word with spaces or `;` (`set name "Big Boss"`); inside quotes, `\"` and `\\` escape a quote
 *   and a backslash. An empty pair of quotes is an empty word.
 * - `//` outside quotes starts a comment up to the end of the line.
 * - Empty statements are dropped.
 *
 * The line is untrusted: it is refused if longer than `MAX_LINE_LENGTH`, if it has more than `MAX_LINE_WORDS` words,
 * if it contains control characters other than tabs, or if a quote is not closed.
 * @param line Line as typed.
 * @return The statements, or an error message for the user.
 */
[[nodiscard]] std::expected<std::vector<Statement>, std::string> splitCommandLine(std::string_view line);

/**
 * @brief Quotes a word if needed so that `splitCommandLine` reads it back unchanged.
 * @param word Word.
 * @return The word, or the word in double quotes with `"` and `\` escaped.
 */
[[nodiscard]] std::string quoteWord(std::string_view word);

}  // namespace tools
