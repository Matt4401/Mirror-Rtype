#include "tools/console/ConsoleHelpers.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "tools/console/CommandLine.hpp"

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

std::string printable(std::string_view text) {
    std::string shown(text.substr(0, MAX_LINE_LENGTH));
    std::ranges::replace_if(shown, isControl, '?');
    if (text.size() > MAX_LINE_LENGTH) {
        shown += "...";
    }
    return shown;
}

bool isBlankLine(std::string_view line) noexcept { return line.find_first_not_of(" \t") == std::string_view::npos; }

std::string commonPrefix(const std::vector<std::string>& words) {
    if (words.empty()) {
        return {};
    }
    std::string_view prefix = words.front();
    for (const std::string& word : words) {
        const auto mismatch = std::ranges::mismatch(prefix, word);
        prefix = prefix.substr(0, static_cast<std::size_t>(mismatch.in1 - prefix.begin()));
    }
    return std::string(prefix);
}

std::optional<WordCursor> locateLastWord(std::string_view line) {
    if (!splitCommandLine(line)) {
        return std::nullopt;
    }
    WordCursor cursor;
    const std::size_t separator = line.find_last_of(" \t;");
    cursor.start = separator == std::string_view::npos ? 0 : separator + 1;
    const std::string_view typed = line.substr(cursor.start);
    if (typed.contains('"') || typed.contains("//")) {
        return std::nullopt;
    }
    const std::string_view before = line.substr(0, cursor.start);
    const std::size_t last = before.find_last_not_of(" \t");
    if (last == std::string_view::npos || before.at(last) == ';') {
        return cursor;
    }
    const auto statements = splitCommandLine(before);
    if (!statements || statements->empty()) {
        return std::nullopt;
    }
    cursor.index = statements->back().size();
    cursor.name = statements->back().front();
    return cursor;
}

}  // namespace tools::detail
