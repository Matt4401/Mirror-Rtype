/**
 * @file PlayerShip.hpp
 * @brief Built-in "player_ship" prefab: the ship controlled by a player.
 */

#pragma once

#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/** @brief Name under which the player ship prefab is registered. */
inline constexpr std::string_view PLAYER_SHIP_PREFAB = "player_ship";

/** @brief Half-width of the player ship hitbox in pixels. */
inline constexpr float PLAYER_SHIP_HALF_WIDTH = 16.0F;

/** @brief Half-height of the player ship hitbox in pixels. */
inline constexpr float PLAYER_SHIP_HALF_HEIGHT = 8.0F;

/**
 * @brief Attaches the player ship components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`) to an entity.
 *
 * The ship moves through its `Transform` (player input), never through the physics solver:
 * - its body is Dynamic, because Box2D only reports contacts when at least one body is Dynamic, and enemies and
 *   power-ups are Kinematic;
 * - its collider is a sensor, so Box2D never pushes it and the client prediction stays exact.
 *
 * The collider belongs to the `PLAYER` category: it touches enemies, enemy shots, obstacles and power-ups.
 *
 * Parameters: the transform keys (see `makeTransform`).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildPlayerShip(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
