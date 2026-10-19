#pragma once

#include <cstdint>

namespace ecs {

/** @brief Simulation modes supported by a 2D rigid body. */
enum class BodyType : std::uint8_t {
    Static,
    Kinematic,
    Dynamic,
};

/**
 * @brief Physical motion properties for an entity.
 *
 * Velocities and limits use pixels per second at the ECS boundary. The physics system converts them
 * to Box2D meters with its shared pixels-per-meter constant.
 */
struct RigidBody2D {
    /** @brief Simulation body type (Static, Kinematic, or Dynamic). */
    BodyType bodyType = BodyType::Static;

    /** @brief Linear velocity along the X axis in pixels per second. */
    float velocityX = 0.0F;

    /** @brief Linear velocity along the Y axis in pixels per second. */
    float velocityY = 0.0F;

    /** @brief Linear damping coefficient applied to reduce speed over time. */
    float linearDamping = 0.0F;

    /** @brief Angular damping coefficient applied to reduce rotational velocity. */
    float angularDamping = 0.0F;

    /** @brief Maximum linear speed threshold in pixels per second. */
    float maxLinearSpeed = 2000.0F;

    /** @brief Whether rotation is locked to prevent physical spinning. */
    bool fixedRotation = true;

    /** @brief Whether continuous collision detection is enabled for fast bullets. */
    bool bullet = false;
};

}  // namespace ecs
