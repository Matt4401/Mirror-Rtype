#pragma once

#include <box2d/b2_body.h>
#include <box2d/b2_math.h>
#include <box2d/b2_world.h>

#include <cstddef>
#include <unordered_map>

#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace systems {

/**
 * @brief Advances Box2D bodies and synchronizes them with ECS transforms.
 *
 * The registry passed to the constructor must outlive this system.
 */
class PhysicsSystem {
  public:
    explicit PhysicsSystem(ecs::Registry& registry);
    ~PhysicsSystem();

    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    PhysicsSystem(PhysicsSystem&&) = delete;
    PhysicsSystem& operator=(PhysicsSystem&&) = delete;

    /** @brief Synchronizes ECS components, advances Box2D, then updates ECS state. */
    void update(float fixedDt);

    /** @brief Number of Box2D bodies currently owned by ECS entities. */
    [[nodiscard]] std::size_t bodyCount() const noexcept { return _bodies.size(); }

  private:
    b2Body* createBody(ecs::Entity entity, const ecs::Transform& transform, const ecs::RigidBody2D& rigidBody);
    void synchronizeBody(ecs::Entity entity, ecs::Transform& transform, ecs::RigidBody2D& rigidBody);
    void synchronizeCollider(ecs::Entity entity, const ecs::Transform& transform, b2Body& body);
    void destroyBody(ecs::Entity entity);

    ecs::Registry* _registry;
    b2World _world;
    std::unordered_map<ecs::Entity, b2Body*> _bodies;
    std::unordered_map<ecs::Entity, b2Vec2> _colliderScales;
    std::size_t _destroyListenerId;
    std::size_t _rigidBodyRemoveListenerId;
};

}  // namespace systems
