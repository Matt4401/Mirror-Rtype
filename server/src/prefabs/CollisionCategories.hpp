/**
 * @file CollisionCategories.hpp
 * @brief Collision categories and masks shared by every prefab.
 *
 * Each category is one bit of `Collider2D::categoryBits` ("what I am").
 * Each mask is the set of categories a collider accepts in `Collider2D::maskBits` ("what I can touch").
 * Two colliders interact only if each one's category is accepted by the other one's mask.
 */

#pragma once

#include <cstdint>

#include "components/Collider2D.hpp"

namespace prefabs::collision {

/** @brief Player-controlled ship. */
inline constexpr std::uint16_t PLAYER = 0x0001;

/** @brief Hostile ship. */
inline constexpr std::uint16_t ENEMY = 0x0002;

/** @brief Projectile fired by a player. */
inline constexpr std::uint16_t PLAYER_SHOT = 0x0004;

/** @brief Projectile fired by an enemy. */
inline constexpr std::uint16_t ENEMY_SHOT = 0x0008;

/** @brief Solid level geometry. */
inline constexpr std::uint16_t OBSTACLE = 0x0010;

/** @brief Collectible bonus. */
inline constexpr std::uint16_t POWER_UP = 0x0020;

/** @brief Mask accepting every category. */
inline constexpr std::uint16_t ALL = 0xFFFF;

/** @brief Categories a player ship interacts with. */
inline constexpr std::uint16_t PLAYER_MASK = ENEMY | ENEMY_SHOT | OBSTACLE | POWER_UP;

/** @brief Categories an enemy ship interacts with. */
inline constexpr std::uint16_t ENEMY_MASK = PLAYER | PLAYER_SHOT;

/** @brief Categories a player projectile interacts with. */
inline constexpr std::uint16_t PLAYER_SHOT_MASK = ENEMY | OBSTACLE;

/** @brief Categories an enemy projectile interacts with. */
inline constexpr std::uint16_t ENEMY_SHOT_MASK = PLAYER | OBSTACLE;

/** @brief Categories an obstacle interacts with. */
inline constexpr std::uint16_t OBSTACLE_MASK = ALL;

/** @brief Categories a power-up interacts with. */
inline constexpr std::uint16_t POWER_UP_MASK = PLAYER;

/**
 * @brief Applies Box2D filtering rules to tell whether two colliders interact.
 *
 * @param a First collider.
 * @param b Second collider.
 * @return True if each collider's category is accepted by the other one's mask.
 */
[[nodiscard]] constexpr bool canCollide(ecs::Collider2D const& a, ecs::Collider2D const& b) noexcept {
    return (a.categoryBits & b.maskBits) != 0 && (b.categoryBits & a.maskBits) != 0;
}

}  // namespace prefabs::collision
