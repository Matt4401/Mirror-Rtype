#include "prefabs/alien_scout/AlienScout.hpp"

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

void buildAlienScout(ecs::Entity e, ecs::Registry& registry, float x, float y, float speed) {
    registry.addComponent<ecs::Transform>(e, ecs::Transform{.x = x, .y = y});
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{.layer = 1});
    registry.addComponent<ecs::RigidBody2D>(e,
                                            ecs::RigidBody2D{.bodyType = ecs::BodyType::Kinematic, .velocityX = speed});
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{.halfWidth = ALIEN_SCOUT_HALF_SIZE,
                                                              .halfHeight = ALIEN_SCOUT_HALF_SIZE,
                                                              .categoryBits = collision::ENEMY,
                                                              .maskBits = collision::ENEMY_MASK,
                                                              .isSensor = true});
}

void registerAlienScoutPrefab(PrefabManager& manager, float speed, std::string_view name) {
    manager.registerPrefab(std::string{name}, [speed](ecs::Entity e, ecs::Registry& registry, float x, float y) {
        buildAlienScout(e, registry, x, y, speed);
    });
}

}  // namespace prefabs
