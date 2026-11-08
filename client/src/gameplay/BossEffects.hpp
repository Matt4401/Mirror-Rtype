/**
 * @file BossEffects.hpp
 * @brief Client-side boss effects: a flash when a boss or a turret loses hit points, explosions when a boss dies.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "components/Renderable.hpp"
#include "ecs/Registry.hpp"

namespace client::gameplay {

/** @brief Seconds a hit entity stays tinted. */
inline constexpr float HIT_FLASH_DURATION = 0.08F;
/** @brief Tint of a hit entity during its flash. */
inline constexpr ecs::Rgba HIT_FLASH_TINT = {.r = 255, .g = 90, .b = 90, .a = 255};
/** @brief Bursts of a boss explosion. */
inline constexpr int EXPLOSION_BURSTS = 9;
/** @brief Seconds between the first and the last burst of a boss explosion. */
inline constexpr float EXPLOSION_SPREAD = 1.2F;
/** @brief Seconds one burst lasts. */
inline constexpr float EXPLOSION_BURST_DURATION = 0.45F;
/** @brief Side of a burst drawn as a box, in pixels, when it appears. */
inline constexpr float EXPLOSION_BURST_SIZE = 40.0F;
/** @brief How many times bigger a box burst is when it ends. */
inline constexpr float EXPLOSION_BURST_GROWTH = 2.5F;

/**
 * @brief Flashes an entity when its replicated hit points go down.
 *
 * Added by the client to the bosses and the turrets. Their `rtype::game::EntityNetState::health` is compared with the
 * last value seen on each frame.
 */
struct HitFlash {
    /** @brief Hit points when the entity appeared. */
    std::uint8_t spawnHealth = 1;
    /** @brief Hit points seen on the previous frame. */
    std::uint8_t lastHealth = 1;
    /** @brief Seconds of flash left. */
    float remaining = 0.0F;
    /** @brief Tint outside of a flash. */
    ecs::Rgba baseTint;
};

/**
 * @brief Starts a flash on each entity whose hit points went down, and tints the entities while they flash.
 * @param registry Client registry.
 * @param deltaTime Seconds since the previous frame.
 */
void updateHitFlashes(ecs::Registry& registry, float deltaTime);

/**
 * @brief Tells whether an entity that left the snapshots was killed, rather than removed with its game.
 *
 * The server destroys an entity in the step its hit points reach 0, so the client last sees it with few hit points
 * left: at most 1, or at most a tenth of its hit points at spawn.
 *
 * @param flash Hit points seen on the entity.
 * @return True if the entity looks killed.
 */
[[nodiscard]] bool diedInCombat(const HitFlash& flash) noexcept;

/** @brief Where and when one burst of a boss explosion appears. */
struct BurstSpec {
    /** @brief Horizontal center, in pixels. */
    float x = 0.0F;
    /** @brief Vertical center, in pixels. */
    float y = 0.0F;
    /** @brief Seconds before it appears. */
    float delay = 0.0F;
};

/**
 * @brief Lays out the bursts of a boss explosion over the boss: `EXPLOSION_BURSTS` bursts, the first one at the center,
 * the others spread over its box and over `EXPLOSION_SPREAD` seconds. Always the same layout for the same box.
 * @param x Horizontal center of the boss.
 * @param y Vertical center of the boss.
 * @param width Width of the boss on screen.
 * @param height Height of the boss on screen.
 * @return The bursts, in the order they appear.
 */
[[nodiscard]] std::vector<BurstSpec> explosionBursts(float x, float y, float width, float height);

/**
 * @brief Damaged looks of a boss or a part: which one it shows, set by the client on the entities whose look has
 * `below_health` stages.
 */
struct LookStage {
    /** @brief Variant of the boss, or look of the part. */
    std::uint8_t id = 0;
    /** @brief True for a boss part, false for a boss. */
    bool part = false;
    /** @brief Stage shown: -1 for the look at full health, else the index of the damaged look. */
    int current = -1;
};

/**
 * @brief Picks the damaged look to show for some hit points.
 * @param thresholds `below_health` of the damaged looks, highest first.
 * @param health Hit points left.
 * @param spawnHealth Hit points at spawn.
 * @return The index of the most damaged look whose threshold the hit points reached, or -1 for the full-health look.
 */
[[nodiscard]] int healthStage(std::span<const float> thresholds, std::uint8_t health,
                              std::uint8_t spawnHealth) noexcept;

/** @brief Hit points of the bosses on screen, for the boss health bar. */
struct BossHealth {
    /** @brief Hit points left, summed over the bosses. */
    float points = 0.0F;
    /** @brief Hit points at spawn, summed over the bosses. */
    float max = 0.0F;
};

/**
 * @brief Sums the hit points of the replicated bosses (`boss_core` entities with a `HitFlash`).
 * @param registry Client registry.
 * @return Their hit points, or nothing when no boss is on screen.
 */
[[nodiscard]] std::optional<BossHealth> bossHealth(const ecs::Registry& registry);

/** @brief One burst of an explosion: hidden during its delay, then it grows, fades and is destroyed. */
struct ExplosionBurst {
    /** @brief Seconds before it appears. */
    float delay = 0.0F;
    /** @brief Seconds since it appeared. */
    float age = 0.0F;
    /** @brief Seconds it lasts once it appeared. */
    float duration = EXPLOSION_BURST_DURATION;
    /** @brief Scale when it appears. */
    float startScale = 1.0F;
    /** @brief Scale reached at the end, as a multiple of `startScale`; 1 for a sprite, which animates on its own. */
    float growth = EXPLOSION_BURST_GROWTH;
    /** @brief Tint when it appears; its opacity goes down to 0. */
    ecs::Rgba tint;
};

/**
 * @brief Plays the explosion bursts: they wait for their delay, grow, fade out and are destroyed at the end.
 * @param registry Client registry.
 * @param deltaTime Seconds since the previous frame.
 */
void updateExplosions(ecs::Registry& registry, float deltaTime);

}  // namespace client::gameplay
