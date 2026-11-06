#pragma once

#include "components/InputBuffer.hpp"
#include "components/Transform.hpp"
#include "input/ActionId.hpp"

namespace rtype::game {

/** @brief Horizontal axis action of a ship: its value is in `NetInput::axes`. */
inline constexpr input::ActionId ACTION_MOVE_X = input::makeActionId("move_x");
/** @brief Vertical axis action of a ship: its value is in `NetInput::axes`. */
inline constexpr input::ActionId ACTION_MOVE_Y = input::makeActionId("move_y");

/** @brief Speed of a ship, in world units per second. */
inline constexpr float SHIP_SPEED = 350.0F;
/** @brief Smallest X position of a ship. */
inline constexpr float SHIP_MIN_X = 32.0F;
/** @brief Largest X position of a ship. */
inline constexpr float SHIP_MAX_X = 1248.0F;
/** @brief Smallest Y position of a ship. */
inline constexpr float SHIP_MIN_Y = 32.0F;
/** @brief Largest Y position of a ship. */
inline constexpr float SHIP_MAX_Y = 688.0F;

/**
 * @brief Moves a ship according to one input.
 *
 * The server calls it to simulate and the client to predict: both must compute the same position. The input comes from
 * the network: an axis outside [-1, 1] cannot make the ship faster, and a diagonal is as fast as a straight line.
 *
 * @param transform Ship to move.
 * @param input Input to apply; actions other than `ACTION_MOVE_X` and `ACTION_MOVE_Y` are ignored.
 * @param deltaTime Duration covered by the input, in seconds.
 */
void applyShipInput(ecs::Transform& transform, const ecs::NetInput& input, float deltaTime);

}  // namespace rtype::game
