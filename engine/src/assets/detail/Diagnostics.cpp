#include "assets/detail/Diagnostics.hpp"

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
    if (_onceKeys.size() < MAX_WARN_ONCE_KEYS) {
        if (!_onceKeys.emplace(key).second) {
            return;
        }
    }
    warn(std::move(message));
}

}  // namespace assets::detail
