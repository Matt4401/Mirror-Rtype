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
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/Health.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

void buildBossCore(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    // Read as a double and clamped before narrowing: any value of a level file (huge, negative, NaN) then fits the
    // network health field, where a direct integer conversion of an out-of-range double would be undefined.
    const double requested = params.get("health", static_cast<double>(DEFAULT_BOSS_CORE_HEALTH));
    const auto health = std::isnan(requested)
                            ? DEFAULT_BOSS_CORE_HEALTH
                            : static_cast<std::uint8_t>(std::clamp(
                                  requested, 1.0, static_cast<double>(std::numeric_limits<std::uint8_t>::max())));

    registry.addComponent<ecs::Transform>(e, makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{
                                                   .bodyType = ecs::BodyType::Kinematic,
                                                   .velocityX = params.get("speed", DEFAULT_BOSS_CORE_SPEED),
                                               });
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = BOSS_CORE_HALF_WIDTH,
                                                  .halfHeight = BOSS_CORE_HALF_HEIGHT,
                                                  .categoryBits = collision::ENEMY,
                                                  .maskBits = collision::ENEMY_MASK,
                                                  .isSensor = true,
                                              });
    registry.addComponent<Health>(e, Health{.points = health, .max = health});
    registry.addComponent<BossCore>(e, BossCore{.stopX = params.get("stop_x", DEFAULT_BOSS_CORE_STOP_X)});
}

}  // namespace prefabs
