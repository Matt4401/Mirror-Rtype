#include "prefabs/missile/Missile.hpp"

#include <string>
#include <string_view>

#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabManager.hpp"

namespace prefabs {

void buildMissile(ecs::Entity e, ecs::Registry& registry, float x, float y, float speed) {
    registry.addComponent<ecs::Transform>(e, ecs::Transform{.x = x, .y = y});
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(
        e, ecs::RigidBody2D{.bodyType = ecs::BodyType::Kinematic, .velocityX = speed, .bullet = true});
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{.halfWidth = 8.0F, .halfHeight = 3.0F, .isSensor = true});
}

void registerMissilePrefab(PrefabManager& manager, float speed, std::string_view name) {
    manager.registerPrefab(std::string{name}, [speed](ecs::Entity e, ecs::Registry& registry, float x, float y) {
        buildMissile(e, registry, x, y, speed);
    });
}

}  // namespace prefabs
