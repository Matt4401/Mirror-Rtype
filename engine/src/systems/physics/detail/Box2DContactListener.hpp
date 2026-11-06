/**
 * @file Box2DContactListener.hpp
 * @brief Private Box2D contact listener of the physics module. Never include it outside `systems/physics/`.
 */

#pragma once

#include <box2d/b2_world_callbacks.h>

#include <cstdint>
#include <vector>

#include "ecs/Entity.hpp"

class b2Contact;

namespace physics::detail {

/** @brief Unique identifier of a Box2D body, never reused, stored in the body user data. */
using BodySerial = std::uintptr_t;

/** @brief One side of a recorded contact. */
struct ContactSide {
    /** @brief Entity owning the fixture (from the fixture user data). */
    ecs::Entity entity = 0;
    /** @brief Serial of the body when the contact was recorded. */
    BodySerial serial = 0;
    /** @brief Collision category of the fixture. */
    std::uint16_t category = 0;
};

/** @brief Contact change recorded during a step, published once the step is over. Plain data, no allocation. */
struct PendingContact {
    /** @brief First fixture side. */
    ContactSide a;
    /** @brief Second fixture side. */
    ContactSide b;
    /** @brief Unit normal from A to B (begin of a solid contact only). */
    float normalX = 0.0F;
    /** @brief Unit normal from A to B (begin of a solid contact only). */
    float normalY = 0.0F;
    /** @brief Contact point in pixels: average of the manifold points (begin of a solid contact only). */
    float pointX = 0.0F;
    /** @brief Contact point in pixels: average of the manifold points (begin of a solid contact only). */
    float pointY = 0.0F;
    /** @brief Whether the normal and point are set (false for sensors and end contacts). */
    bool hasContactPoint = false;
    /** @brief Whether one of the fixtures is a sensor. */
    bool isSensor = false;
    /** @brief True for a begin, false for an end. */
    bool begin = true;
};

/**
 * @brief Box2D contact listener buffering contact changes into a reserved vector.
 *
 * Box2D calls it inside `b2World::Step`, where the world is locked: it only records plain data, and
 * `PhysicsSystem` publishes the contacts once the step is over. The buffer keeps its capacity between steps, so
 * recording does not allocate once it reached its high-water mark.
 */
class Box2DContactListener final : public b2ContactListener {
  public:
    /** @brief Reserves the contact buffer. */
    Box2DContactListener();

    /** @brief Records the begin of a contact, with its point and normal when it has a manifold. */
    void BeginContact(b2Contact* contact) override;

    /** @brief Records the end of a contact. */
    void EndContact(b2Contact* contact) override;

    /** @brief Ignores contact changes while set (bodies being destroyed). */
    void setSuppressed(bool suppressed) noexcept { _suppressed = suppressed; }

    /**
     * @brief Swaps the recorded contacts with `out` (expected empty), without allocating.
     * @param out Receives the recorded contacts; its previous storage becomes the new (cleared) buffer.
     */
    void swapPending(std::vector<PendingContact>& out) noexcept;

  private:
    /** @brief Records a contact change unless suppressed. */
    void record(b2Contact& contact, bool begin);

    /** @brief Contacts recorded since the last swap. */
    std::vector<PendingContact> _pending;
    /** @brief Whether contact changes are currently ignored. */
    bool _suppressed = false;
};

}  // namespace physics::detail
