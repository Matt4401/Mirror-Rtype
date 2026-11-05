#pragma once

#include <cstddef>

namespace physics {

/** @brief Number of screen pixels represented by one Box2D meter. */
inline constexpr float PPM = 32.0F;

/** @brief Box2D solver iterations used for velocity constraints per fixed step. */
inline constexpr int VELOCITY_ITERATIONS = 8;

/** @brief Box2D solver iterations used for position constraints per fixed step. */
inline constexpr int POSITION_ITERATIONS = 3;

/** @brief Maximum transform difference tolerated before teleporting a Box2D body. */
inline constexpr float POSITION_EPSILON = 0.0001F;

/** @brief Contacts the physics system can buffer per step before its contact buffers have to grow. */
inline constexpr std::size_t INITIAL_CONTACT_CAPACITY = 256;

}  // namespace physics
