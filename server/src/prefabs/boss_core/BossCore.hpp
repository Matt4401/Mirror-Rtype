/**
 * @file BossCore.hpp
 * @brief Built-in "boss_core" prefab: a large armored enemy ending a level, that takes many hits to destroy.
 *
 * The prefab only holds the boss body. Its moves and attacks come from the Wren script named by the level `boss`
 * event; without a script, it flies in from the right and stops at `stop_x`.
 */

#pragma once

#include <cstdint>
#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/** @brief Name under which the boss core prefab is registered. */
inline constexpr std::string_view BOSS_CORE_PREFAB = "boss_core";

/** @brief Default horizontal speed of the boss while it flies in, in pixels per second (negative: to the left). */
inline constexpr float DEFAULT_BOSS_CORE_SPEED = -120.0F;

/** @brief Default abscissa at which the boss stops flying in, in pixels. */
inline constexpr float DEFAULT_BOSS_CORE_STOP_X = 1000.0F;

/** @brief Default hit points of the boss. */
inline constexpr std::uint8_t DEFAULT_BOSS_CORE_HEALTH = 40;

/** @brief Half-width of the boss hitbox in pixels, shared with the clients. */
inline constexpr float BOSS_CORE_HALF_WIDTH = rtype::game::BOSS_CORE_WIDTH / 2.0F;

/** @brief Half-height of the boss hitbox in pixels, shared with the clients. */
inline constexpr float BOSS_CORE_HALF_HEIGHT = rtype::game::BOSS_CORE_HEIGHT / 2.0F;

/**
 * @brief Marks a boss and holds where it stops flying in.
 *
 * The gameplay stops the boss once it reaches `stopX`, and resumes the level when no boss is left.
 */
struct BossCore {
    /** @brief Abscissa at which the boss stops flying in. */
    float stopX = DEFAULT_BOSS_CORE_STOP_X;
};

/**
 * @brief Attaches the boss components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`, `Health`, `BossCore`)
 * to an entity.
 *
 * The collider belongs to the `ENEMY` category: it hits the player and is hit by player shots.
 *
 * Parameters: the transform keys (see `makeTransform`), `speed` (horizontal velocity while flying in, in pixels per
 * second, `DEFAULT_BOSS_CORE_SPEED` by default), `stop_x` (`DEFAULT_BOSS_CORE_STOP_X` by default) and `health`
 * (hit points, clamped to 1–255, `DEFAULT_BOSS_CORE_HEALTH` by default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildBossCore(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
