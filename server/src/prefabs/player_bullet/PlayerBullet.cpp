#include "prefabs/player_bullet/PlayerBullet.hpp"

#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

void buildPlayerBullet(ecs::Entity e, ecs::Registry& registry, PrefabParams const& params) {
    registry.addComponent<ecs::Transform>(e, makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(
        e, ecs::RigidBody2D{.bodyType = ecs::BodyType::Dynamic,
                            .velocityX = params.get("speed", DEFAULT_PLAYER_BULLET_SPEED),
                            .bullet = true});
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = 6.0F,
                                                  .halfHeight = 2.0F,
                                                  .categoryBits = collision::PLAYER_SHOT,
                                                  .maskBits = collision::PLAYER_SHOT_MASK,
                                                  .isSensor = true,
                                              });
}

}  // namespace prefabs
