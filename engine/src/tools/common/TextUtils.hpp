/**
 * @file TextUtils.hpp
 * @brief Internal ASCII text helpers shared by the cvars and the console.
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace tools::detail {

/**
 * @brief Tells whether a byte is an ASCII control character (tabs and newlines included).
 * @param c Byte.
 * @return `true` for bytes below 0x20 and for 0x7F.
 */
[[nodiscard]] bool isControl(char c) noexcept;

/**
 * @brief Lowercases ASCII letters.
 * @param text Text.
 * @return A lowercase copy; other bytes are unchanged.
 */
[[nodiscard]] std::string toLower(std::string_view text);

/**
 * @brief Tells whether a byte continues a UTF-8 sequence.
 * @param c Byte.
 * @return `true` for bytes of the form 10xxxxxx.
 */
[[nodiscard]] bool isUtf8Continuation(char c) noexcept;

/**
 * @brief Encodes a code point in UTF-8.
 * @param cp Unicode code point.
 * @return The 1 to 4 bytes; empty for a surrogate or a value above U+10FFFF.
 */
[[nodiscard]] std::string encodeUtf8(char32_t cp);

/**
 * @brief Finds the start of the code point before a position.
 * @param text UTF-8 text.
 * @param pos Byte position, at most `text.size()`.
 * @return Byte position of the previous code point, or 0.
 */
[[nodiscard]] std::size_t previousCodePoint(std::string_view text, std::size_t pos) noexcept;

/**
 * @brief Finds the start of the code point after a position.
 * @param text UTF-8 text.
 * @param pos Byte position, at most `text.size()`.
 * @return Byte position of the next code point, or `text.size()`.
 */
[[nodiscard]] std::size_t nextCodePoint(std::string_view text, std::size_t pos) noexcept;

}  // namespace tools::detail
