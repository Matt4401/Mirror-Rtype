#include "prefabs/obstacle/Obstacle.hpp"

#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

void buildObstacle(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    registry.addComponent<ecs::Transform>(e, makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{});
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{.bodyType = ecs::BodyType::Static});
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = params.get("halfWidth", DEFAULT_OBSTACLE_HALF_WIDTH),
                                                  .halfHeight = params.get("halfHeight", DEFAULT_OBSTACLE_HALF_HEIGHT),
                                                  .categoryBits = collision::OBSTACLE,
                                                  .maskBits = collision::OBSTACLE_MASK,
                                              });
}

}  // namespace prefabs
