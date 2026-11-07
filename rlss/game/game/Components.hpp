/**
 * @file Components.hpp
 * @brief RLSS game components and constants shared by the server and the client.
 */

#pragma once

#include <cstdint>
#include <string_view>

#include "ecs/Entity.hpp"
#include "input/ActionId.hpp"

namespace rlss::game {

/** @brief `ecs::Replicated::kind` of a car. */
inline constexpr std::uint32_t KIND_CAR = 1;

/** @brief Horizontal axis action in [-1, 1]: drive on the ground, rotate in the air. */
inline constexpr std::string_view ACTION_STEER = "steer";
/** @brief Jump button action. */
inline constexpr std::string_view ACTION_JUMP = "jump";
/** @brief Id of `ACTION_STEER`. */
inline constexpr input::ActionId STEER_ID = input::makeActionId(ACTION_STEER);
/** @brief Id of `ACTION_JUMP`. */
inline constexpr input::ActionId JUMP_ID = input::makeActionId(ACTION_JUMP);

/** @brief Tag (`ecs::Tag::name`) of the entity carrying the `ArenaShape`, found by the Wren scripts. */
inline constexpr std::string_view ARENA_TAG = "arena";

/** @brief Wren module (file path without `.wren`, relative to the working directory) of the car physics. */
inline constexpr std::string_view CAR_SCRIPT_MODULE = "rlss/scripts/Car";
/** @brief Wren module of the client-side car visuals. */
inline constexpr std::string_view CAR_VIEW_SCRIPT_MODULE = "rlss/scripts/CarView";

/** @brief Look of a car: its index in the player palette (`rlss::playerColor`). Replicated. */
struct CarLook {
    /** @brief Palette index. */
    std::uint8_t color = 0;
};

/** @brief Geometry of the arena, read by the Wren physics through the `ArenaShape` class. */
struct ArenaShape {
    /** @brief Left wall. */
    float left = 0.0F;
    /** @brief Ceiling. */
    float top = 0.0F;
    /** @brief Right wall. */
    float right = 0.0F;
    /** @brief Floor. */
    float bottom = 0.0F;
    /** @brief Radius of the curved corners. */
    float cornerRadius = 0.0F;
};

/** @brief Client: entities drawing the parts of a car, placed every frame by the `CarView` script. */
struct CarParts {
    /** @brief Chassis, in the player color. */
    ecs::Entity body = 0;
    /** @brief Cabin, on top of the chassis. */
    ecs::Entity cabin = 0;
    /** @brief Rear wheel. */
    ecs::Entity rearWheel = 0;
    /** @brief Front wheel. */
    ecs::Entity frontWheel = 0;
    /** @brief Headlight, shows where the car is facing. */
    ecs::Entity headlight = 0;
};

}  // namespace rlss::game
