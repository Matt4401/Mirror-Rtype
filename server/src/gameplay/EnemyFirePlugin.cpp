#include "gameplay/EnemyFirePlugin.hpp"

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
            _shots.push_back({.from = from, .velocity = aimAt(from.x, from.y, target, gun.shotSpeed)});
        });
    // Spawned after the walk: adding entities while iterating the guns could move the storage under it.
    for (const Shot& shot : _shots) {
        spawnEnemyMissile(registry, shot.from.x, shot.from.y, shot.velocity);
    }
}

}  // namespace rtype::server::gameplay
