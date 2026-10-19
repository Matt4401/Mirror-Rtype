#pragma once

#include <cstdint>

namespace ecs {

/** @brief Geometries supported by a 2D collider. */
enum class ColliderShape : std::uint8_t {
    Box,
    Circle,
};

/**
 * @brief Collision geometry and material properties for an entity.
 *
 * Dimensions use pixels at the ECS boundary. Box dimensions are half-extents.
 */
struct Collider2D {
    /** @brief Geometric primitive used for collision detection. */
    ColliderShape shape = ColliderShape::Box;

    /** @brief Half-width extents in pixels for Box shapes. */
    float halfWidth = 16.0F;

    /** @brief Half-height extents in pixels for Box shapes. */
    float halfHeight = 16.0F;

    /** @brief Radius in pixels for Circle shapes. */
    float radius = 16.0F;

    /** @brief Surface friction coefficient. */
    float friction = 0.2F;

    /** @brief Restitution (bounciness) coefficient. */
    float restitution = 0.0F;

    /** @brief Collision category bitfield for filtering. */
    std::uint16_t categoryBits = 0x0001;

    /** @brief Collision mask bitfield specifying categories this collider reacts to. */
    std::uint16_t maskBits = 0xFFFF;

    /** @brief Flag indicating if the collider is a non-solid trigger/sensor. */
    bool isSensor = false;
};

}  // namespace ecs
