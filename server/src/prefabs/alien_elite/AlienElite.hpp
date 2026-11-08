/**
 * @file AlienElite.hpp
 * @brief Built-in "alien_elite" prefab: a rare, tough enemy that weaves, fires fans of shots and drops a power-up.
 */

#pragma once

#include <cstdint>
#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/** @brief Name under which the alien elite prefab is registered. */
inline constexpr std::string_view ALIEN_ELITE_PREFAB = "alien_elite";

/** @brief Default horizontal speed of an alien elite, in pixels per second (negative: towards the players). */
inline constexpr float DEFAULT_ALIEN_ELITE_SPEED = -70.0F;

/** @brief Default hit points of an alien elite. */
inline constexpr std::uint8_t DEFAULT_ALIEN_ELITE_HEALTH = 6;

/** @brief Default half-height of the weave of an alien elite, in pixels. */
inline constexpr float DEFAULT_ALIEN_ELITE_AMPLITUDE = 90.0F;

/** @brief Default weaves per second of an alien elite. */
inline constexpr float DEFAULT_ALIEN_ELITE_FREQUENCY = 0.35F;

/** @brief Default seconds between two fans of shots of an alien elite. */
inline constexpr float DEFAULT_ALIEN_ELITE_FIRE_INTERVAL = 2.4F;

/** @brief Default speed of the shots of an alien elite, in pixels per second. */
inline constexpr float DEFAULT_ALIEN_ELITE_SHOT_SPEED = 260.0F;

/** @brief Half-side of the alien elite hitbox, in pixels. */
inline constexpr float ALIEN_ELITE_HALF_SIZE = 28.0F;

/**
 * @brief Attaches the alien elite components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`, `Health`,
 * `SineWave`, `EnemyGun` with a `GunPattern::Spread`, `DropsPowerUp`) to an entity.
 *
 * Parameters: the transform keys (see `makeTransform`), `speed` (`DEFAULT_ALIEN_ELITE_SPEED` by default), `health`
 * (clamped to 1–255, `DEFAULT_ALIEN_ELITE_HEALTH` by default), `amplitude` (clamped to 0–360 px) and `frequency`
 * (clamped to 0–4 waves per second) of its weave, `fire_interval` (clamped to 0.3–10 s) and `shot_speed` (clamped to
 * 50–900 px/s) of its gun.
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildAlienElite(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
