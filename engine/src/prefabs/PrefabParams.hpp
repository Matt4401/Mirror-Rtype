/**
 * @file PrefabParams.hpp
 * @brief Named, loosely typed parameters passed to a prefab factory (position, rotation, speed, size...).
 */

#pragma once

#include "components/Transform.hpp"
#include "ecs/ParamBag.hpp"

namespace prefabs {

namespace detail {

/** @brief Transparent string hash, shared with `ecs::ParamBag`. */
using ecs::detail::StringHash;

}  // namespace detail

/**
 * @brief Key/value bag describing how a prefab instance must be configured: an `ecs::ParamBag`.
 *
 * Each prefab reads the keys it understands and falls back to its own defaults for the missing ones, so a prefab can
 * take any number of parameters without changing the `PrefabManager` API.
 *
 * @code
 * manager.spawn("missile", buffer, {{"x", 800.0F}, {"y", 300.0F}, {"rotation", 0.5F}, {"speed", -900.0F}});
 * @endcode
 */
using PrefabParams = ecs::ParamBag;

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
