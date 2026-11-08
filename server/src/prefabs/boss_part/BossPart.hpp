/**
 * @file BossPart.hpp
 * @brief Built-in "boss_part" prefab: a piece of a boss (weak point, turret or armor) that moves with it.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

/** @brief Name under which the boss part prefab is registered. */
inline constexpr std::string_view BOSS_PART_PREFAB = "boss_part";

/** @brief Default side of a boss part hitbox, in pixels. */
inline constexpr float DEFAULT_BOSS_PART_SIZE = 48.0F;

/** @brief Default hit points of a turret. */
inline constexpr std::uint8_t DEFAULT_TURRET_HEALTH = 8;

/** @brief What a boss part does when a player shot hits it. */
enum class BossPartRole : std::uint8_t {
    /** @brief The hit goes to the boss. A boss with a weak point only takes damage through it. */
    WeakPoint,
    /** @brief The part has its own hit points and is destroyed alone; the boss keeps fighting. */
    Turret,
    /** @brief The shot stops on it, without any damage. */
    Armor,
};

/** @brief Name of `BossPartRole::WeakPoint` in level files. */
inline constexpr std::string_view BOSS_PART_WEAK_POINT = "weak_point";
/** @brief Name of `BossPartRole::Turret` in level files. */
inline constexpr std::string_view BOSS_PART_TURRET = "turret";
/** @brief Name of `BossPartRole::Armor` in level files. */
inline constexpr std::string_view BOSS_PART_ARMOR = "armor";

/**
 * @brief Reads a part role from its name in a level file.
 * @param name `weak_point`, `turret` or `armor`.
 * @return The role, or nothing for any other name.
 */
[[nodiscard]] std::optional<BossPartRole> bossPartRole(std::string_view name);

/**
 * @brief Marks a piece of a boss.
 *
 * The part is a child of its boss in the entity hierarchy: it follows the boss, and it is destroyed with it.
 */
struct BossPart {
    /** @brief What a player shot does to it. */
    BossPartRole role = BossPartRole::Armor;
    /** @brief Look of the part, sent to the clients as its `subtype`. */
    std::uint8_t look = 0;
};

/**
 * @brief Attaches the boss part components (`Transform`, `Renderable`, `RigidBody2D`, `Collider2D`, `BossPart`, and
 * `Health` for a turret) to an entity.
 *
 * The collider belongs to the `ENEMY` category, like the boss: it hits the player and is hit by player shots.
 *
 * Parameters: the transform keys (see `makeTransform`), `role` (`weak_point`, `turret` or `armor`; any other value
 * gives an armor), `width` and `height` (hitbox size in pixels, clamped to 8–1024, `DEFAULT_BOSS_PART_SIZE` by
 * default), `health` (hit points of a turret, clamped to 1–255, `DEFAULT_TURRET_HEALTH` by default) and `look`
 * (clamped to 0–255, 0 by default).
 *
 * @param e Freshly spawned entity.
 * @param registry Registry owning the entity.
 * @param params Spawn parameters.
 */
void buildBossPart(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params);

}  // namespace prefabs
