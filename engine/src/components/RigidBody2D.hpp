#pragma once

namespace ecs {

/** @brief Simulation modes supported by a 2D rigid body. */
enum class BodyType {
    Static,
    Kinematic,
    Dynamic,
};

/**
 * @brief Physical motion properties for an entity.
 *
 * Velocities and limits use pixels per second at the ECS boundary. The physics system converts them to
 * Box2D meters with its shared pixels-per-meter constant.
 */
struct RigidBody2D {
    BodyType bodyType = BodyType::Static;
    float velocityX = 0.0F;
    float velocityY = 0.0F;
    float linearDamping = 0.0F;
    float angularDamping = 0.0F;
    float maxLinearSpeed = 2000.0F;
    bool fixedRotation = true;
    bool bullet = false;
};

}  // namespace ecs