#include "gameplay/EnemyFirePlugin.hpp"

#include <cmath>

#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "gameplay/GameplayComponents.hpp"
#include "gameplay/Targeting.hpp"
#include "prefabs/EnemyGun.hpp"

namespace rtype::server::gameplay {

namespace {

/**
 * @brief Tells whether a position is on the screen.
 * @param transform Position.
 * @return True inside the scene.
 */
bool onScreen(const ecs::Transform& transform) noexcept {
    return transform.x > 0.0F && transform.x < SCENE_WIDTH && transform.y > 0.0F && transform.y < SCENE_HEIGHT;
}

/**
 * @brief Turns a velocity by an angle.
 * @param velocity Velocity.
 * @param angle Angle, in radians.
 * @return The turned velocity, at the same speed.
 */
Velocity rotated(const Velocity velocity, const float angle) noexcept {
    const float cos = std::cos(angle);
    const float sin = std::sin(angle);
    return {.x = (velocity.x * cos) - (velocity.y * sin), .y = (velocity.x * sin) + (velocity.y * cos)};
}

}  // namespace

void EnemyFirePlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    ecs::Registry& registry = engine.registry();
    _shots.clear();
    registry.runSystem<::prefabs::EnemyGun, ecs::Transform>(
        [this, &registry, &time](ecs::Entity /*entity*/, ::prefabs::EnemyGun& gun, const ecs::Transform& transform) {
            if (!onScreen(transform)) {
                return;
            }
            gun.cooldown -= time.deltaTime;
            if (gun.cooldown > 0.0F) {
                return;
            }
            gun.cooldown += gun.interval;
            const AimPoint from{.x = transform.x - MUZZLE_OFFSET, .y = transform.y};
            const AimPoint target = nearestShip(registry, from.x, from.y).value_or(AimPoint{.x = 0.0F, .y = from.y});
            const Velocity aimed = aimAt(from.x, from.y, target, gun.shotSpeed);
            _shots.push_back({.from = from, .velocity = aimed});
            if (gun.pattern == ::prefabs::GunPattern::Spread) {
                _shots.push_back({.from = from, .velocity = rotated(aimed, ::prefabs::SPREAD_ANGLE)});
                _shots.push_back({.from = from, .velocity = rotated(aimed, -::prefabs::SPREAD_ANGLE)});
            }
        });
    // Spawned after the walk: adding entities while iterating the guns could move the storage under it.
    for (const Shot& shot : _shots) {
        spawnEnemyMissile(registry, shot.from.x, shot.from.y, shot.velocity);
    }
}

}  // namespace rtype::server::gameplay
