#include "assets/AssetPath.hpp"

#include <algorithm>
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
    std::string normalized = std::filesystem::path(generic).lexically_normal().generic_string();
    while (normalized.size() > 1 && normalized.ends_with('/')) {
        normalized.pop_back();
    }
    if (normalized.empty() || normalized == ".") {
        return std::nullopt;
    }
    return normalized;
}

bool isValidAssetName(const std::string_view name) noexcept {
    return !name.empty() && name.size() <= MAX_NAME_LENGTH && !name.contains('\0');
}

}  // namespace assets
