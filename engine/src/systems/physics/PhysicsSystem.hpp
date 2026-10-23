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
    /**
     * @brief Constructs physics system and hooks lifecycle listeners on registry.
     * @param registry Target ECS registry holding physics entities.
     */
    explicit PhysicsSystem(ecs::Registry& registry);

    /** @brief Cleans up Box2D world and detaches lifecycle listeners. */
    ~PhysicsSystem();

    /** @brief Deleted copy constructor. */
    PhysicsSystem(const PhysicsSystem&) = delete;

    /** @brief Deleted copy assignment operator. */
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    /** @brief Deleted move constructor. */
    PhysicsSystem(PhysicsSystem&&) = delete;

    /** @brief Deleted move assignment operator. */
    PhysicsSystem& operator=(PhysicsSystem&&) = delete;

    /** @brief Synchronizes ECS components, advances Box2D, then updates ECS state. */
    void update(float fixedDt);

    /** @brief Number of Box2D bodies currently owned by ECS entities. */
    [[nodiscard]] std::size_t bodyCount() const noexcept { return _bodies.size(); }

  private:
    /** @brief Creates a Box2D body for a given entity. */
    b2Body* createBody(ecs::Entity entity, const ecs::Transform& transform, const ecs::RigidBody2D& rigidBody);

    /** @brief Synchronizes Box2D body velocities and states with ECS component. */
    void synchronizeBody(ecs::Entity entity, ecs::Transform& transform, ecs::RigidBody2D& rigidBody);

    /** @brief Synchronizes entity collider fixture with Box2D body. */
    void synchronizeCollider(ecs::Entity entity, const ecs::Transform& transform, b2Body& body);

    /** @brief Destroys and cleans up Box2D body associated with entity. */
    void destroyBody(ecs::Entity entity);

    /** @brief Pointer to target ECS registry. */
    ecs::Registry* _registry;

    /** @brief Box2D physical simulation world. */
    b2World _world;

    /** @brief Map of entity IDs to Box2D body handles. */
    std::unordered_map<ecs::Entity, b2Body*> _bodies;

    /** @brief Map of entity IDs to cached collider scale factors. */
    std::unordered_map<ecs::Entity, b2Vec2> _colliderScales;

    /** @brief Listener token for entity destruction events. */
    std::size_t _destroyListenerId;

    /** @brief Listener token for RigidBody component removal events. */
    std::size_t _rigidBodyRemoveListenerId;
};

}  // namespace systems
