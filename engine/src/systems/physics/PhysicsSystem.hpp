#pragma once

#include <cstddef>
#include <unordered_map>

#include <box2d/box2d.h>

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
    void synchronizeCollider(ecs::Entity entity, b2Body& body);
    void destroyBody(ecs::Entity entity);

    ecs::Registry* _registry;
    b2World _world;
    std::unordered_map<ecs::Entity, b2Body*> _bodies;
    std::size_t _destroyListenerId;
};

}  // namespace systems