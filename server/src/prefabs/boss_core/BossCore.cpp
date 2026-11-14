#include "prefabs/boss_core/BossCore.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "prefabs/CollisionCategories.hpp"
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

void buildBossCore(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    // Read as doubles and clamped before narrowing: any value of a level file then fits the network byte fields, where
    // a direct integer conversion of an out-of-range double would be undefined.
    constexpr auto BYTE_MAX = static_cast<double>(std::numeric_limits<std::uint8_t>::max());
    const auto health = static_cast<std::uint8_t>(
        clampedParam(params, "health", static_cast<double>(DEFAULT_BOSS_CORE_HEALTH), 1.0, BYTE_MAX));
    const auto variant = static_cast<std::uint8_t>(clampedParam(params, "variant", 0.0, 0.0, BYTE_MAX));
    const auto width = static_cast<float>(
        clampedParam(params, "width", rtype::game::BOSS_CORE_WIDTH, MIN_BOSS_CORE_SIZE, MAX_BOSS_CORE_SIZE));
    const auto height = static_cast<float>(
        clampedParam(params, "height", rtype::game::BOSS_CORE_HEIGHT, MIN_BOSS_CORE_SIZE, MAX_BOSS_CORE_SIZE));

    registry.addComponent<ecs::Transform>(e, makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{
                                                   .bodyType = ecs::BodyType::Kinematic,
                                                   .velocityX = params.get("speed", DEFAULT_BOSS_CORE_SPEED),
                                               });
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = width / 2.0F,
                                                  .halfHeight = height / 2.0F,
                                                  .categoryBits = collision::ENEMY,
                                                  .maskBits = collision::ENEMY_MASK,
                                                  .isSensor = true,
                                              });
    registry.addComponent<Health>(e, Health{.points = health, .max = health});
    registry.addComponent<BossCore>(
        e, BossCore{.stopX = params.get("stop_x", DEFAULT_BOSS_CORE_STOP_X), .variant = variant});
}

}  // namespace prefabs
