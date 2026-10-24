/**
 * @file PrefabParams.hpp
 * @brief Named, loosely typed parameters passed to a prefab factory (position, rotation, speed, size...).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

#include "components/Transform.hpp"

namespace prefabs {

namespace detail {

/** @brief Transparent hash allowing `std::string_view` lookups without allocation. */
struct StringHash {
    /** @brief Transparent lookup tag for std::unordered_map string view hashing. */
    using is_transparent = void;

    /**
     * @brief Hashes a string view without heap allocation.
     * @param s String view to hash.
     * @return Computed hash value.
     */
    [[nodiscard]] std::size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
};

}  // namespace detail

/**
 * @brief Key/value bag describing how a prefab instance must be configured.
 *
 * Each prefab reads the keys it understands and falls back to its own defaults for the missing ones,
 * so a prefab can take any number of parameters without changing the `PrefabManager` API.
 * Values are stored as `bool`, `std::int64_t`, `double` or `std::string`, which matches what scripts
 * and network messages can carry.
 *
 * @code
 * manager.spawn("missile", buffer, {{"x", 800.0F}, {"y", 300.0F}, {"rotation", 0.5F}, {"speed", -900.0F}});
 * @endcode
 */
class PrefabParams {
  public:
    /** @brief Storage type of a single parameter. */
    using Value = std::variant<bool, std::int64_t, double, std::string>;

    /** @brief Constructs an empty parameter bag. */
    PrefabParams() = default;

    /**
     * @brief Builds the bag from a list of `{key, value}` pairs.
     * @param values Initial parameters; a repeated key keeps its first value.
     */
    PrefabParams(std::initializer_list<std::pair<std::string const, Value>> values) : _values(values) {}

    /**
     * @brief Sets (or replaces) a parameter.
     * @param key Parameter name.
     * @param value Parameter value.
     * @return This bag, to chain calls.
     */
    PrefabParams& set(std::string_view key, Value value) {
        _values.insert_or_assign(std::string{key}, std::move(value));
        return *this;
    }

    /**
     * @brief Checks whether a parameter is present, whatever its type.
     * @param key Parameter name.
     * @return True if the key exists.
     */
    [[nodiscard]] bool has(std::string_view key) const { return _values.contains(key); }

    /**
     * @brief Reads a parameter as `T`.
     *
     * Numeric values convert between each other (an `int` stored for a `float` key is accepted);
     * `bool` and `std::string` only match themselves.
     *
     * @tparam T Requested type.
     * @param key Parameter name.
     * @return The value, or `std::nullopt` if the key is missing or holds an incompatible type.
     */
    template <typename T>
    [[nodiscard]] std::optional<T> find(std::string_view key) const {
        auto const it = _values.find(key);
        if (it == _values.end()) {
            return std::nullopt;
        }
        return std::visit(
            [](auto const& stored) -> std::optional<T> {
                using Stored = std::decay_t<decltype(stored)>;
                if constexpr (std::is_same_v<Stored, T>) {
                    return stored;
                } else if constexpr (isNumber<Stored> && isNumber<T>) {
                    return static_cast<T>(stored);
                } else {
                    return std::nullopt;
                }
            },
            it->second);
    }

    /**
     * @brief Reads a parameter as `T`, or returns a fallback.
     * @tparam T Requested type.
     * @param key Parameter name.
     * @param fallback Value returned if the key is missing or holds an incompatible type.
     * @return The parameter value or `fallback`.
     */
    template <typename T>
    [[nodiscard]] T get(std::string_view key, T fallback) const {
        return find<T>(key).value_or(std::move(fallback));
    }

    /**
     * @brief Copies every parameter of `overrides` into this bag, replacing existing keys.
     * @param overrides Parameters taking precedence.
     */
    void merge(PrefabParams const& overrides) {
        for (auto const& [key, value] : overrides._values) {
            _values.insert_or_assign(key, value);
        }
    }

  private:
    /** @brief Arithmetic types that convert between each other (`bool` excluded). */
    template <typename T>
    static constexpr bool isNumber = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

    /** @brief Parameters indexed by name. */
    std::unordered_map<std::string, Value, detail::StringHash, std::equal_to<>> _values;
};

/**
 * @brief Builds a `Transform` from the standard spatial keys.
 *
 * Reads `x`, `y`, `z`, `rotation` (radians), `scaleX` and `scaleY`; missing keys keep the `Transform` defaults.
 *
 * @param params Spawn parameters.
 * @return The resulting transform.
 */
[[nodiscard]] inline ecs::Transform makeTransform(PrefabParams const& params) {
    ecs::Transform const defaults{};
    return ecs::Transform{
        .x = params.get("x", defaults.x),
        .y = params.get("y", defaults.y),
        .z = params.get("z", defaults.z),
        .rotation = params.get("rotation", defaults.rotation),
        .scaleX = params.get("scaleX", defaults.scaleX),
        .scaleY = params.get("scaleY", defaults.scaleY),
    };
}

}  // namespace prefabs
