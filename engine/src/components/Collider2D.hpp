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
    ColliderShape shape = ColliderShape::Box;
    float halfWidth = 16.0F;
    float halfHeight = 16.0F;
    float radius = 16.0F;
    float density = 1.0F;
    float friction = 0.2F;
    float restitution = 0.0F;
    std::uint16_t categoryBits = 0x0001;
    std::uint16_t maskBits = 0xFFFF;
    bool isSensor = false;
};

}  // namespace ecs
