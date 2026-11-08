/**
 * @file CVarRef.hpp
 * @brief Typed handle on a registered CVar.
 */

#pragma once

#include <expected>
#include <string>

#include "tools/cvars/CVar.hpp"
#include "tools/cvars/CVarTypes.hpp"

namespace tools {

class CVarManager;

namespace detail {

/**
 * @brief Changes a variable from code through its manager (bounds checked, flags ignored).
 *
 * Defined with `CVarManager`, so that `CVarRef` does not need the full manager.
 * @param manager Owner of the variable.
 * @param cvar Variable.
 * @param value New value, of the type of the variable.
 * @return An error message if the value is refused.
 */
std::expected<void, std::string> assignCVar(CVarManager& manager, CVar& cvar, CVarValue value);

}  // namespace detail

/**
 * @brief Typed handle on a registered CVar, kept by the code that owns the setting.
 *
 * Reading it is a pointer access: systems read it every frame and see console changes immediately. Valid as long as
 * the `CVarManager` that returned it (variables are never removed).
 * @tparam T Type of the variable.
 */
template <CVarType T>
class CVarRef {
  public:
    /** @brief Creates an empty handle; using it before assigning a registered one is undefined. */
    CVarRef() = default;

    /** @brief Gets the current value. @return Current value. */
    [[nodiscard]] const T& get() const { return _cvar->get<T>(); }

    /** @brief Gets the variable. @return The variable behind the handle. */
    [[nodiscard]] const CVar& cvar() const noexcept { return *_cvar; }

    /**
     * @brief Changes the value from code: flags do not apply, bounds do.
     * @param value New value.
     * @return An error message if the value is out of bounds or invalid.
     */
    std::expected<void, std::string> set(T value) const;

    /** @brief Tells whether the handle points to a variable. @return `true` once assigned. */
    [[nodiscard]] explicit operator bool() const noexcept { return _cvar != nullptr; }

  private:
    friend class CVarManager;

    /**
     * @brief Creates a handle.
     * @param manager Owner of the variable.
     * @param cvar Variable.
     */
    CVarRef(CVarManager* manager, CVar* cvar) noexcept : _manager(manager), _cvar(cvar) {}

    /** @brief Owner of the variable. */
    CVarManager* _manager = nullptr;
    /** @brief Variable, stable in the manager's map. */
    CVar* _cvar = nullptr;
};

}  // namespace tools

#include "tools/cvars/CVarRef.tpp"  // IWYU pragma: export
