/**
 * @file PowerUp.hpp
 * @brief Built-in "power_up" prefab: a collectible slowly drifting to the left.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/** @brief Name under which the power-up prefab is registered. */
inline constexpr std::string_view POWER_UP_PREFAB = "power_up";

/** @brief Default horizontal drift speed of a power-up in pixels per second (negative: scrolls to the left). */
inline constexpr float DEFAULT_POWER_UP_DRIFT_SPEED = -100.0F;

/** @brief Radius of the power-up pickup area in pixels. */
inline constexpr float POWER_UP_RADIUS = 12.0F;

/**
 * @brief Attaches the power-up components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`) to an entity.
 *
 * The collider is a sensor: it detects the player without physically blocking it.
 *
 * Parameters: the transform keys (see `makeTransform`) and `speed` (horizontal drift in pixels per second,
 * `DEFAULT_POWER_UP_DRIFT_SPEED` by default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildPowerUp(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
