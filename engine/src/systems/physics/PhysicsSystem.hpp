#pragma once

#include <cstddef>
#include <memory>

#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"

namespace systems {

/**
 * @brief Advances the physics simulation and synchronizes it with ECS transforms.
 *
 * Entities with a `Transform` and a `RigidBody2D` get a physics body; a `Collider2D` gives that body a shape.
 * Contacts found during a step are published on the EventBus as `events::CollisionBeginEvent` and
 * `events::CollisionEndEvent` once the step is over (see `events/PhysicsEvents.hpp`).
 *
 * The physics backend (Box2D) is a private implementation detail: this header exposes engine types only.
 * The registry and the event bus passed to the constructor must outlive this system.
 */
class PhysicsSystem {
  public:
    /**
     * @brief Constructs the physics system and hooks lifecycle listeners on the registry.
     * @param registry Target ECS registry holding physics entities.
     * @param bus Event bus receiving the contact events.
     */
    PhysicsSystem(ecs::Registry& registry, ecs::EventBus& bus);

    /** @brief Detaches lifecycle listeners and releases the physics world. */
    ~PhysicsSystem();

    /** @brief Deleted copy constructor. */
    PhysicsSystem(const PhysicsSystem&) = delete;

    /** @brief Deleted copy assignment operator. */
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    /** @brief Deleted move constructor. */
    PhysicsSystem(PhysicsSystem&&) = delete;

    /** @brief Deleted move assignment operator. */
    PhysicsSystem& operator=(PhysicsSystem&&) = delete;

    /**
     * @brief Synchronizes ECS components, advances the simulation, updates ECS state, then publishes contact events.
     * @param fixedDt Duration of the step in seconds; non-positive values are ignored.
     */
    void update(float fixedDt);

    /** @brief Number of physics bodies currently owned by ECS entities. */
    [[nodiscard]] std::size_t bodyCount() const noexcept;

  private:
    struct Impl;

    /** @brief Backend state (physics world, entity-body mapping, contact recorder). */
    std::unique_ptr<Impl> _impl;
};

}  // namespace systems
