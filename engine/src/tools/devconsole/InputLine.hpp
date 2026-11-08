/**
 * @file InputLine.hpp
 * @brief Editable single line of UTF-8 text with a cursor.
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace tools {

/**
 * @brief Editable line of UTF-8 text with a cursor, as typed in the developer console.
 *
 * The cursor is a byte position, always on a code point boundary. Control characters and invalid code points are
 * refused, and the line never grows past its byte limit, so its content is always safe to execute and draw.
 */
class InputLine {
  public:
    /**
     * @brief Creates an empty line.
     * @param maxBytes Longest accepted text, in bytes.
     */
    explicit InputLine(std::size_t maxBytes) noexcept : _maxBytes(maxBytes) {}

    /** @brief Gets the text. @return Text, UTF-8. */
    [[nodiscard]] const std::string& text() const noexcept { return _text; }

    /** @brief Gets the cursor. @return Byte position of the cursor. */
    [[nodiscard]] std::size_t cursor() const noexcept { return _cursor; }

    /**
     * @brief Inserts a character at the cursor.
     * @param cp Unicode code point.
     * @return `false` if the character is a control character, invalid, or does not fit.
     */
    bool insert(char32_t cp);

    /** @brief Erases the character before the cursor (Backspace). */
    void eraseBackward();

    /** @brief Erases the character after the cursor (Delete). */
    void eraseForward();

    /** @brief Moves the cursor one character left. */
    void moveLeft() noexcept;

    /** @brief Moves the cursor one character right. */
    void moveRight() noexcept;

    /** @brief Moves the cursor to the start. */
    void moveHome() noexcept { _cursor = 0; }

    /** @brief Moves the cursor to the end. */
    void moveEnd() noexcept { _cursor = _text.size(); }

    /**
     * @brief Replaces the text and puts the cursor at its end.
     * @param text New text, cut at the byte limit on a code point boundary.
     */
    void assign(std::string_view text);

    /** @brief Empties the line. */
    void clear() noexcept;

  private:
    /** @brief Longest accepted text, in bytes. */
    std::size_t _maxBytes;
    /** @brief Text, UTF-8. */
    std::string _text;
    /** @brief Byte position of the cursor. */
    std::size_t _cursor = 0;
};

}  // namespace tools
