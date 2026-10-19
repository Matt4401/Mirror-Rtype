#include "prefabs/power_up/PowerUp.hpp"

#include <string>
#include <string_view>

#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabManager.hpp"

namespace prefabs {

void buildPowerUp(ecs::Entity e, ecs::Registry& registry, float x, float y, float driftSpeed) {
    registry.addComponent<ecs::Transform>(e, ecs::Transform{.x = x, .y = y});
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(
        e, ecs::RigidBody2D{.bodyType = ecs::BodyType::Kinematic, .velocityX = driftSpeed});
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{.shape = ecs::ColliderShape::Circle,
                                                              .radius = POWER_UP_RADIUS,
                                                              .categoryBits = collision::POWER_UP,
                                                              .maskBits = collision::POWER_UP_MASK,
                                                              .isSensor = true});
}

void registerPowerUpPrefab(PrefabManager& manager, float driftSpeed, std::string_view name) {
    manager.registerPrefab(std::string{name}, [driftSpeed](ecs::Entity e, ecs::Registry& registry, float x, float y) {
        buildPowerUp(e, registry, x, y, driftSpeed);
    });
}

}  // namespace prefabs
