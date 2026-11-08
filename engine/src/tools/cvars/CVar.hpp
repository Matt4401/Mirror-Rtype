/**
 * @file CVar.hpp
 * @brief One console variable: name, description, rules, default and current value.
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "tools/cvars/CVarTypes.hpp"

namespace tools {

/**
 * @brief One console variable. Read-only view: values change through `CVarManager` or a `CVarRef`.
 */
class CVar {
  public:
    /**
     * @brief Creates a variable at its default value.
     * @param name Unique name.
     * @param defaultValue Default value; its type is the type of the variable.
     * @param description Help text.
     * @param options Flags and bounds.
     */
    CVar(std::string name, CVarValue defaultValue, std::string description, CVarOptions options);

    /** @brief Gets the name. @return Name of the variable. */
    [[nodiscard]] const std::string& name() const noexcept { return _name; }

    /** @brief Gets the help text. @return Description given at registration. */
    [[nodiscard]] const std::string& description() const noexcept { return _description; }

    /** @brief Gets the rules. @return Flags given at registration. */
    [[nodiscard]] CVarFlag flags() const noexcept { return _options.flags; }

    /** @brief Gets the bounds. @return Bounds of a numeric variable, if any. */
    [[nodiscard]] const std::optional<CVarRange>& range() const noexcept { return _options.range; }

    /** @brief Gets the current value. @return Current value. */
    [[nodiscard]] const CVarValue& value() const noexcept { return _value; }

    /** @brief Gets the default value. @return Value given at registration. */
    [[nodiscard]] const CVarValue& defaultValue() const noexcept { return _default; }

    /**
     * @brief Tells whether the variable holds a type.
     * @tparam T Type to test.
     * @return `true` if the variable was registered with a `T` default.
     */
    template <CVarType T>
    [[nodiscard]] bool holds() const noexcept {
        return std::holds_alternative<T>(_value);
    }

    /**
     * @brief Gets the current value with its type.
     * @tparam T Type of the variable.
     * @return Current value.
     * @throws std::bad_variant_access If the variable does not hold a `T`.
     */
    template <CVarType T>
    [[nodiscard]] const T& get() const {
        return std::get<T>(_value);
    }

    /** @brief Gets the name of the type. @return `bool`, `int`, `float` or `string`. */
    [[nodiscard]] std::string_view typeName() const noexcept;

  private:
    friend class CVarManager;

    /** @brief Unique name. */
    std::string _name;
    /** @brief Help text. */
    std::string _description;
    /** @brief Flags and bounds. */
    CVarOptions _options;
    /** @brief Value given at registration. */
    CVarValue _default;
    /** @brief Current value, always of the same alternative as `_default`. */
    CVarValue _value;
};

}  // namespace tools
