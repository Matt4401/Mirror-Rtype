/**
 * @file CVarTypes.hpp
 * @brief Plain types describing a CVar: flags, value, accepted types, bounds and registration options.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace tools {

/**
 * @brief Rules attached to a CVar, combined with `|`.
 */
enum class CVarFlag : std::uint8_t {
    /** @brief No rule. */
    None = 0,
    /** @brief Changed from the console only while cheats are allowed; reset to its default when they get disabled. */
    Cheat = 1U << 0U,
    /** @brief Meant to be saved between sessions (the owner of the persistence reads this flag). */
    Archived = 1U << 1U,
    /** @brief Readable from the console, but only the code that registered it may change it. */
    ReadOnly = 1U << 2U,
};

/**
 * @brief Combines two flags.
 * @param lhs First flag set.
 * @param rhs Second flag set.
 * @return The union of both sets.
 */
[[nodiscard]] constexpr CVarFlag operator|(CVarFlag lhs, CVarFlag rhs) noexcept {
    return static_cast<CVarFlag>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

/**
 * @brief Tells whether a flag set contains a flag.
 * @param flags Flag set.
 * @param flag Flag to look for.
 * @return `true` if every bit of `flag` is set in `flags`.
 */
[[nodiscard]] constexpr bool hasFlag(CVarFlag flags, CVarFlag flag) noexcept {
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) == static_cast<std::uint8_t>(flag);
}

/** @brief Value of a CVar: the alternative held fixes the type of the variable for its whole life. */
using CVarValue = std::variant<bool, int, float, std::string>;

/**
 * @brief Types a CVar can hold.
 * @tparam T Candidate type.
 */
template <typename T>
concept CVarType =
    std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, float> || std::is_same_v<T, std::string>;

/**
 * @brief Inclusive bounds of a numeric CVar.
 */
struct CVarRange {
    /** @brief Smallest accepted value. */
    double min;
    /** @brief Largest accepted value. */
    double max;
};

/**
 * @brief Optional settings of a CVar given at registration.
 */
struct CVarOptions {
    /** @brief Rules of the variable. */
    CVarFlag flags = CVarFlag::None;
    /** @brief Bounds of an `int` or `float` variable; ignored for the other types. */
    std::optional<CVarRange> range;
};

}  // namespace tools
