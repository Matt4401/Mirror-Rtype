#include "tools/console/CommandLine.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tools/common/TextUtils.hpp"

namespace tools {

namespace {

[[nodiscard]] bool isBlank(char c) noexcept { return c == ' ' || c == '\t'; }

/** @brief Control characters refused in a line: all but the tab, which separates words. */
[[nodiscard]] bool isForbiddenControl(char c) noexcept { return c != '\t' && detail::isControl(c); }

/** @brief Reads a line character by character into statements. */
class Splitter {
  public:
    explicit Splitter(std::string_view line) noexcept : _line(line) {}

    std::expected<std::vector<Statement>, std::string> run() {
        while (_pos < _line.size()) {
            if (auto read = _quoted ? readQuoted() : readPlain(); !read) {
                return std::unexpected(std::move(read.error()));
            }
        }
        if (_quoted) {
            return std::unexpected(std::string("Unterminated quote"));
        }
        if (auto ended = endWord(); !ended) {
            return std::unexpected(std::move(ended.error()));
        }
        endStatement();
        return std::move(_statements);
    }

  private:
    /** @brief Character at an offset from the cursor, or `\0` past the end (never in a valid line). */
    [[nodiscard]] char peek(std::size_t offset = 0) const noexcept {
        return _pos + offset < _line.size() ? _line.at(_pos + offset) : '\0';
    }

    std::expected<void, std::string> readQuoted() {
        const char c = peek();
        const char next = peek(1);
        if (c == '\\' && (next == '"' || next == '\\')) {
            append(next);
            _pos += 2;
            return {};
        }
        if (c == '"') {
            _quoted = false;
        } else {
            append(c);
        }
        ++_pos;
        return {};
    }

    std::expected<void, std::string> readPlain() {
        const char c = peek();
        if (c == '/' && peek(1) == '/') {
            _pos = _line.size();
            return {};
        }
        ++_pos;
        if (c == '"') {
            _quoted = true;
            _inWord = true;
            return {};
        }
        if (!isBlank(c) && c != ';') {
            append(c);
            return {};
        }
        auto ended = endWord();
        if (ended && c == ';') {
            endStatement();
        }
        return ended;
    }

    std::expected<void, std::string> endWord() {
        if (!_inWord) {
            return {};
        }
        if (++_words > MAX_LINE_WORDS) {
            return std::unexpected("Too many words (at most " + std::to_string(MAX_LINE_WORDS) + ")");
        }
        _current.push_back(std::exchange(_word, {}));
        _inWord = false;
        return {};
    }

    void endStatement() {
        if (!_current.empty()) {
            _statements.push_back(std::exchange(_current, {}));
        }
    }

    void append(char c) {
        _word.push_back(c);
        _inWord = true;
    }

    std::string_view _line;
    std::size_t _pos = 0;
    std::vector<Statement> _statements;
    Statement _current;
    std::string _word;
    std::size_t _words = 0;
    bool _inWord = false;
    bool _quoted = false;
};

}  // namespace

std::expected<void, std::string> validateCommandLine(std::string_view line) {
    if (line.size() > MAX_LINE_LENGTH) {
        return std::unexpected("Line too long (at most " + std::to_string(MAX_LINE_LENGTH) + " characters)");
    }
    if (std::ranges::any_of(line, isForbiddenControl)) {
        return std::unexpected(std::string("Line contains control characters"));
    }
    return {};
}

std::expected<std::vector<Statement>, std::string> splitCommandLine(std::string_view line) {
    if (auto valid = validateCommandLine(line); !valid) {
        return std::unexpected(std::move(valid.error()));
    }
    return Splitter(line).run();
}

std::string quoteWord(std::string_view word) {
    const bool plain =
        !word.empty() && word.find_first_of(" \t;\"\\") == std::string_view::npos && !word.contains("//");
    if (plain) {
        return std::string(word);
    }
    std::string quoted = "\"";
    for (const char c : word) {
        if (c == '"' || c == '\\') {
            quoted.push_back('\\');
        }
        quoted.push_back(c);
    }
    quoted.push_back('"');
    return quoted;
}

}  // namespace tools
