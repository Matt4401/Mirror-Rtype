/**
 * @file PowerUp.hpp
 * @brief Built-in "power_up" prefab: a collectible slowly drifting to the left.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabManager.hpp"

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
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param x Initial horizontal position.
 * @param y Initial vertical position.
 * @param driftSpeed Horizontal velocity in pixels per second.
 */
void buildPowerUp(ecs::Entity e, ecs::Registry& registry, float x, float y, float driftSpeed);

/**
 * @brief Registers a power-up prefab drifting at the given speed.
 *
 * @param manager Prefab manager receiving the factory.
 * @param driftSpeed Horizontal velocity in pixels per second.
 * @param name Name under which the prefab is registered.
 */
void registerPowerUpPrefab(PrefabManager& manager, float driftSpeed = DEFAULT_POWER_UP_DRIFT_SPEED,
                           std::string_view name = POWER_UP_PREFAB);

}  // namespace prefabs
