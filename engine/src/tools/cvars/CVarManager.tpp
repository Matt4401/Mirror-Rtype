/**
 * @file CVarManager.tpp
 * @brief Template implementation of CVarManager.
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "tools/cvars/CVar.hpp"
#include "tools/cvars/CVarManager.hpp"
#include "tools/cvars/CVarRef.hpp"
#include "tools/cvars/CVarTypes.hpp"

namespace tools {

template <CVarType T>
CVarRef<T> CVarManager::add(std::string name, T defaultValue, std::string description, CVarOptions options) {
    CVar& cvar = addImpl(CVar(std::move(name), CVarValue{std::move(defaultValue)}, std::move(description), options));
    return CVarRef<T>{this, &cvar};
}

template <CVarType T>
std::optional<CVarRef<T>> CVarManager::ref(std::string_view name) noexcept {
    const auto it = _cvars.find(name);
    if (it == _cvars.end() || !it->second.holds<T>()) {
        return std::nullopt;
    }
    return CVarRef<T>{this, &it->second};
}

}  // namespace tools
