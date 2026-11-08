#include "prefabs/alien_elite/AlienElite.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/EnemyGun.hpp"
#include "prefabs/EnemyMotion.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

namespace {

/**
 * @brief Reads a number parameter and clamps it, whatever a level file gives (huge, negative, NaN).
 * @param params Spawn parameters.
 * @param key Parameter name.
 * @param fallback Value when the key is missing or NaN.
 * @param low Smallest value.
 * @param high Largest value.
 * @return The value, within [low, high].
 */
double clampedParam(PrefabParams const& params, const char* key, const double fallback, const double low,
                    const double high) {
    const double requested = params.get(key, fallback);
    return std::isnan(requested) ? fallback : std::clamp(requested, low, high);
}

}  // namespace

void buildAlienElite(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    const ecs::Transform transform = makeTransform(params);
    const auto health = static_cast<std::uint8_t>(
        clampedParam(params, "health", static_cast<double>(DEFAULT_ALIEN_ELITE_HEALTH), 1.0, 255.0));
    const auto interval =
        static_cast<float>(clampedParam(params, "fire_interval", DEFAULT_ALIEN_ELITE_FIRE_INTERVAL, 0.3, 10.0));

    registry.addComponent<ecs::Transform>(e, transform);
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{
                                                   .bodyType = ecs::BodyType::Kinematic,
                                                   .velocityX = params.get("speed", DEFAULT_ALIEN_ELITE_SPEED),
                                               });
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = ALIEN_ELITE_HALF_SIZE,
                                                  .halfHeight = ALIEN_ELITE_HALF_SIZE,
                                                  .categoryBits = collision::ENEMY,
                                                  .maskBits = collision::ENEMY_MASK,
                                                  .isSensor = true,
                                              });
    registry.addComponent<Health>(e, Health{.points = health, .max = health});
    registry.addComponent<SineWave>(
        e,
        SineWave{
            .amplitude =
                static_cast<float>(clampedParam(params, "amplitude", DEFAULT_ALIEN_ELITE_AMPLITUDE, 0.0, 360.0)),
            .frequency = static_cast<float>(clampedParam(params, "frequency", DEFAULT_ALIEN_ELITE_FREQUENCY, 0.0, 4.0)),
            .baseY = transform.y,
        });
    registry.addComponent<EnemyGun>(e, EnemyGun{
                                           .interval = interval,
                                           .cooldown = interval / 2.0F,
                                           .shotSpeed = static_cast<float>(clampedParam(
                                               params, "shot_speed", DEFAULT_ALIEN_ELITE_SHOT_SPEED, 50.0, 900.0)),
                                           .pattern = GunPattern::Spread,
                                       });
    registry.addComponent<DropsPowerUp>(e, DropsPowerUp{});
}

}  // namespace prefabs
