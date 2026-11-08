/**
 * @file EnemyGun.hpp
 * @brief Gun of an R-Type enemy that fires on its own, without a script.
 */

#pragma once

#include <cstdint>

namespace prefabs {

/** @brief How an enemy gun aims. */
enum class GunPattern : std::uint8_t {
    /** @brief One shot at the nearest ship. */
    Aimed,
    /** @brief Three shots in a fan: at the nearest ship, and `SPREAD_ANGLE` radians on each side. */
    Spread,
};

/** @brief Angle between two shots of a `GunPattern::Spread` fan, in radians (about 17 degrees). */
inline constexpr float SPREAD_ANGLE = 0.3F;

/**
 * @brief Fires missiles every `interval` seconds while the enemy is on the screen.
 *
 * Read by `gameplay::EnemyFirePlugin`, which spawns the replicated missiles.
 */
struct EnemyGun {
    /** @brief Seconds between two shots. */
    float interval = 2.0F;
    /** @brief Seconds left before the next shot. */
    float cooldown = 1.0F;
    /** @brief Speed of the missiles, in pixels per second; the shots go for the nearest ship. */
    float shotSpeed = 300.0F;
    /** @brief How the shots aim. */
    GunPattern pattern = GunPattern::Aimed;
};

}  // namespace prefabs
