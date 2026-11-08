#include "tools/devconsole/InputLine.hpp"

#include <cstddef>
#include <string>
#include <string_view>

#include "tools/common/TextUtils.hpp"

namespace tools {

bool InputLine::insert(char32_t cp) {
    if (cp < U'\x20' || cp == U'\x7F') {
        return false;
    }
    const std::string bytes = detail::encodeUtf8(cp);
    if (bytes.empty() || _text.size() + bytes.size() > _maxBytes) {
        return false;
    }
    _text.insert(_cursor, bytes);
    _cursor += bytes.size();
    return true;
}

void InputLine::eraseBackward() {
    const std::size_t start = detail::previousCodePoint(_text, _cursor);
    _text.erase(start, _cursor - start);
    _cursor = start;
}

void InputLine::eraseForward() {
    const std::size_t end = detail::nextCodePoint(_text, _cursor);
    _text.erase(_cursor, end - _cursor);
}

void InputLine::moveLeft() noexcept { _cursor = detail::previousCodePoint(_text, _cursor); }

void InputLine::moveRight() noexcept { _cursor = detail::nextCodePoint(_text, _cursor); }

void InputLine::assign(std::string_view text) {
    if (text.size() > _maxBytes) {
        text = text.substr(0, detail::previousCodePoint(text, _maxBytes + 1));
    }
    _text = text;
    _cursor = _text.size();
}

void InputLine::clear() noexcept {
    _text.clear();
    _cursor = 0;
}

}  // namespace tools
