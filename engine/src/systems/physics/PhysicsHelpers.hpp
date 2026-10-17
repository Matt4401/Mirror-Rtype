#pragma once

#include <box2d/b2_body.h>

#include "components/RigidBody2D.hpp"

namespace physics {

/** @brief Maximum transform difference tolerated before teleporting a Box2D body. */
inline constexpr float POSITION_EPSILON = 0.0001F;

/** @brief Converts an ECS rigid-body mode to its Box2D equivalent. */
inline b2BodyType toBox2DBodyType(ecs::BodyType bodyType) {
    switch (bodyType) {
        case ecs::BodyType::Static:
            return b2_staticBody;
        case ecs::BodyType::Kinematic:
            return b2_kinematicBody;
        case ecs::BodyType::Dynamic:
            return b2_dynamicBody;
    }
    return b2_staticBody;
}

}  // namespace physics
