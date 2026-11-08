/**
 * @file CVarRef.tpp
 * @brief Template implementation of CVarRef.
 */

#pragma once

#include <expected>
#include <string>
#include <utility>

#include "tools/cvars/CVarRef.hpp"
#include "tools/cvars/CVarTypes.hpp"

namespace tools {

template <CVarType T>
std::expected<void, std::string> CVarRef<T>::set(T value) const {
    return detail::assignCVar(*_manager, *_cvar, CVarValue{std::move(value)});
}

}  // namespace tools
