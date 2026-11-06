/**
 * @file PhysicsEvents.hpp
 * @brief Contact events published by the physics module on the EventBus.
 */

#pragma once

#include <cstdint>

#include "ecs/Entity.hpp"

namespace events {

/**
 * @brief Two colliders started touching (or overlapping, when one of them is a sensor).
 *
 * Published by the physics system after the simulation step, never from inside it, so subscribers may freely spawn,
 * kill or modify entities. The order of `entityA` and `entityB` is unspecified: use the category bits to tell them
 * apart. A pair is only reported if Box2D filtering accepts it (category/mask) and at least one body is Dynamic.
 */
struct CollisionBeginEvent {
    /** @brief First entity of the contact. */
    ecs::Entity entityA = 0;

    /** @brief Second entity of the contact. */
    ecs::Entity entityB = 0;

    /** @brief `Collider2D::categoryBits` of the first entity. */
    std::uint16_t categoryA = 0;

    /** @brief `Collider2D::categoryBits` of the second entity. */
    std::uint16_t categoryB = 0;

    /** @brief Whether at least one of the two colliders is a sensor (overlap only, no physical response). */
    bool isSensor = false;

    /**
     * @brief Whether `normalX/Y` and `pointX/Y` are set.
     *
     * False for sensor contacts: Box2D computes no contact manifold for them, only the overlap.
     */
    bool hasContactPoint = false;

    /** @brief X component of the unit contact normal, pointing from A to B. */
    float normalX = 0.0F;

    /** @brief Y component of the unit contact normal, pointing from A to B. */
    float normalY = 0.0F;

    /** @brief X of the impact point in pixels (average of the contact points). */
    float pointX = 0.0F;

    /** @brief Y of the impact point in pixels (average of the contact points). */
    float pointY = 0.0F;
};

/**
 * @brief Two colliders stopped touching.
 *
 * Same entity, category and sensor fields and guarantees as `CollisionBeginEvent`, without contact point or normal:
 * the fixtures no longer touch. It is not published when the contact ends because one of the
 * entities (or its body) was destroyed: listen to the registry destruction instead.
 */
struct CollisionEndEvent {
    /** @brief First entity of the contact. */
    ecs::Entity entityA = 0;

    /** @brief Second entity of the contact. */
    ecs::Entity entityB = 0;

    /** @brief `Collider2D::categoryBits` of the first entity. */
    std::uint16_t categoryA = 0;

    /** @brief `Collider2D::categoryBits` of the second entity. */
    std::uint16_t categoryB = 0;

    /** @brief Whether at least one of the two colliders is a sensor. */
    bool isSensor = false;
};

}  // namespace events
