#include "prefabs/obstacle/Obstacle.hpp"

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

void buildObstacle(ecs::Entity e, ecs::Registry& registry, float x, float y, float halfWidth, float halfHeight) {
    registry.addComponent<ecs::Transform>(e, ecs::Transform{.x = x, .y = y});
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{});
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{.bodyType = ecs::BodyType::Static});
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = halfWidth,
                                                  .halfHeight = halfHeight,
                                                  .categoryBits = collision::OBSTACLE,
                                                  .maskBits = collision::OBSTACLE_MASK,
                                              });
}

void registerObstaclePrefab(PrefabManager& manager, float halfWidth, float halfHeight, std::string_view name) {
    manager.registerPrefab(std::string{name},
                           [halfWidth, halfHeight](ecs::Entity e, ecs::Registry& registry, float x, float y) {
                               buildObstacle(e, registry, x, y, halfWidth, halfHeight);
                           });
}

}  // namespace prefabs
