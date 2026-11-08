#include "assets/AssetPath.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace assets {

std::optional<std::string> normalizeAssetPath(const std::string_view path) {
    if (path.empty() || path.size() > MAX_PATH_LENGTH || path.contains('\0')) {
        return std::nullopt;
    }
    std::string generic(path);
    std::ranges::replace(generic, '\\', '/');
    std::string normalized = toUtf8(std::filesystem::path(generic).lexically_normal());
    while (normalized.size() > 1 && normalized.ends_with('/')) {
        normalized.pop_back();
    }
    if (normalized.empty() || normalized == ".") {
        return std::nullopt;
    }
    return normalized;
}

bool isConfinedAssetPath(const std::string_view normalized) noexcept {
    if (normalized.empty() || normalized.starts_with('/')) {
        return false;
    }
    // A drive letter ("C:/", "c:x"): absolute or drive-relative on Windows, never inside the working directory.
    if (normalized.size() >= 2 && normalized[1] == ':' &&
        std::isalpha(static_cast<unsigned char>(normalized[0])) != 0) {
        return false;
    }
    return normalized != ".." && !normalized.starts_with("../");
}

bool isValidAssetName(const std::string_view name) noexcept {
    return !name.empty() && name.size() <= MAX_NAME_LENGTH && !name.contains('\0');
}

bool isValidAssetAlias(const std::string_view alias) noexcept {
    return isValidAssetName(alias) && !looksLikeAssetPath(alias);
}

bool looksLikeAssetPath(const std::string_view aliasOrPath) noexcept {
    return aliasOrPath.find_first_of("/\\.") != std::string_view::npos;
}

std::string toUtf8(const std::filesystem::path& path) {
    const std::u8string text = path.generic_u8string();
    return {text.begin(), text.end()};
}

std::string quoteForLog(const std::string_view text, const std::size_t limit) {
    if (text.size() <= limit) {
        return "'" + std::string(text) + "'";
    }
    return "'" + std::string(text.substr(0, limit)) + "...'";
}

}  // namespace assets
