#include "prefabs/alien_scout/AlienScout.hpp"

#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

void buildAlienScout(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    registry.addComponent<ecs::Transform>(e, makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(
        e, ecs::RigidBody2D{.bodyType = ecs::BodyType::Kinematic,
                            .velocityX = params.get("speed", DEFAULT_ALIEN_SCOUT_SPEED)});
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = ALIEN_SCOUT_HALF_SIZE,
                                                  .halfHeight = ALIEN_SCOUT_HALF_SIZE,
                                                  .categoryBits = collision::ENEMY,
                                                  .maskBits = collision::ENEMY_MASK,
                                                  .isSensor = true,
                                              });
}

}  // namespace prefabs
