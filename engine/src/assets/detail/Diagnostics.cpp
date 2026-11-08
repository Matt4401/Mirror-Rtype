#include "assets/detail/Diagnostics.hpp"

#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace assets::detail {

void Diagnostics::warn(std::string message) {
    std::cerr << "[assets] " << message << '\n';
    if (_warnings.size() >= MAX_KEPT_WARNINGS) {
        _warnings.pop_front();
    }
    _warnings.push_back(std::move(message));
}

void Diagnostics::warnOnce(const std::string_view key, std::string message) {
    const std::size_t hash = std::hash<std::string_view>{}(key);
    if (_onceKeys.contains(hash)) {
        return;
    }
    if (_onceKeys.size() < MAX_WARN_ONCE_KEYS) {
        _onceKeys.insert(hash);
    }
    warn(std::move(message));
}

}  // namespace assets::detail
