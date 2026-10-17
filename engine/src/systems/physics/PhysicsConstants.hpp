#pragma once

namespace physics {

/** @brief Number of screen pixels represented by one Box2D meter. */
inline constexpr float PPM = 32.0F;

/** @brief Box2D solver iterations used for velocity constraints per fixed step. */
inline constexpr int VELOCITY_ITERATIONS = 8;

/** @brief Box2D solver iterations used for position constraints per fixed step. */
inline constexpr int POSITION_ITERATIONS = 3;

}  // namespace physics
