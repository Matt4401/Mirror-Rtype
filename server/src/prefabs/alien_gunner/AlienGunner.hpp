/**
 * @file AlienGunner.hpp
 * @brief Built-in "alien_gunner" prefab: a slow enemy that fires at the ships.
 */

#pragma once

#include <cstdint>
#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/** @brief Name under which the alien gunner prefab is registered. */
inline constexpr std::string_view ALIEN_GUNNER_PREFAB = "alien_gunner";

/** @brief Default horizontal speed of an alien gunner, in pixels per second (negative: towards the players). */
inline constexpr float DEFAULT_ALIEN_GUNNER_SPEED = -90.0F;

/** @brief Default seconds between two shots of an alien gunner. */
inline constexpr float DEFAULT_ALIEN_GUNNER_FIRE_INTERVAL = 1.8F;

/** @brief Default speed of the shots of an alien gunner, in pixels per second. */
inline constexpr float DEFAULT_ALIEN_GUNNER_SHOT_SPEED = 300.0F;

/** @brief Look of the shots of an alien gunner (`EnemyGun::shotLook`). */
inline constexpr std::uint8_t ALIEN_GUNNER_SHOT_LOOK = 1;

/** @brief Half-side of the alien gunner hitbox, in pixels. */
inline constexpr float ALIEN_GUNNER_HALF_SIZE = 20.0F;

/**
 * @brief Attaches the alien gunner components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`, `EnemyGun`) to
 * an entity.
 *
 * Like the alien scout, it dies on the first hit; its `EnemyGun` fires a missile at the nearest ship every
 * `fire_interval` seconds once it is on the screen, the first one half an interval after it appears.
 *
 * Parameters: the transform keys (see `makeTransform`), `speed` (`DEFAULT_ALIEN_GUNNER_SPEED` by default),
 * `fire_interval` (clamped to 0.3–10 s, `DEFAULT_ALIEN_GUNNER_FIRE_INTERVAL` by default) and `shot_speed` (clamped to
 * 50–900 px/s, `DEFAULT_ALIEN_GUNNER_SHOT_SPEED` by default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildAlienGunner(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
