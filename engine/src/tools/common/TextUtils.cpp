#include "tools/common/TextUtils.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace tools::detail {

bool isControl(char c) noexcept {
    const auto byte = static_cast<unsigned char>(c);
    return byte < 0x20U || byte == 0x7FU;
}

std::string toLower(std::string_view text) {
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(),
                           [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; });
    return lower;
}

bool isUtf8Continuation(char c) noexcept { return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U; }

std::string encodeUtf8(char32_t cp) {
    const auto value = static_cast<std::uint32_t>(cp);
    const auto byte = [](std::uint32_t bits) { return static_cast<char>(bits); };
    if (value < 0x80U) {
        return {byte(value)};
    }
    if (value < 0x800U) {
        return {byte(0xC0U | (value >> 6U)), byte(0x80U | (value & 0x3FU))};
    }
    if (value >= 0xD800U && value <= 0xDFFFU) {
        return {};
    }
    if (value < 0x10000U) {
        return {byte(0xE0U | (value >> 12U)), byte(0x80U | ((value >> 6U) & 0x3FU)), byte(0x80U | (value & 0x3FU))};
    }
    if (value <= 0x10FFFFU) {
        return {
            byte(0xF0U | (value >> 18U)),
            byte(0x80U | ((value >> 12U) & 0x3FU)),
            byte(0x80U | ((value >> 6U) & 0x3FU)),
            byte(0x80U | (value & 0x3FU)),
        };
    }
    return {};
}

std::size_t previousCodePoint(std::string_view text, std::size_t pos) noexcept {
    pos = std::min(pos, text.size());
    while (pos > 0) {
        --pos;
        if (!isUtf8Continuation(text.at(pos))) {
            break;
        }
    }
    return pos;
}

std::size_t nextCodePoint(std::string_view text, std::size_t pos) noexcept {
    if (pos >= text.size()) {
        return text.size();
    }
    ++pos;
    while (pos < text.size() && isUtf8Continuation(text.at(pos))) {
        ++pos;
    }
    return pos;
}

}  // namespace tools::detail
