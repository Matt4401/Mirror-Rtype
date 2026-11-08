#include "assets/detail/Diagnostics.hpp"

#include <iostream>
#include <string>
#include <utility>

namespace assets::detail {

void Diagnostics::warn(std::string message) {
    std::cerr << "[assets] " << message << '\n';
    if (_warnings.size() >= MAX_KEPT_WARNINGS) {
        _warnings.erase(_warnings.begin());
    }
    _warnings.push_back(std::move(message));
}

}  // namespace assets::detail
