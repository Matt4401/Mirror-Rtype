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
/** @brief `ecs::Replicated::kind` of the ball. */
inline constexpr std::uint32_t KIND_BALL = 2;

/** @brief Horizontal axis action in [-1, 1]: drive on the ground, rotate in the air. */
inline constexpr std::string_view ACTION_STEER = "steer";
/** @brief Jump button action. */
inline constexpr std::string_view ACTION_JUMP = "jump";
/** @brief Boost button action. */
inline constexpr std::string_view ACTION_BOOST = "boost";
/** @brief Id of `ACTION_STEER`. */
inline constexpr input::ActionId STEER_ID = input::makeActionId(ACTION_STEER);
/** @brief Id of `ACTION_JUMP`. */
inline constexpr input::ActionId JUMP_ID = input::makeActionId(ACTION_JUMP);
/** @brief Id of `ACTION_BOOST`. */
inline constexpr input::ActionId BOOST_ID = input::makeActionId(ACTION_BOOST);

/** @brief Tag (`ecs::Tag::name`) of the entity carrying the `ArenaShape`, found by the Wren scripts. */
inline constexpr std::string_view ARENA_TAG = "arena";

/** @brief Wren module (file path without `.wren`, relative to the working directory) of the match simulation. */
inline constexpr std::string_view MATCH_SCRIPT_MODULE = "rlss/scripts/Match";
/** @brief Wren module of the car physics. */
inline constexpr std::string_view CAR_SCRIPT_MODULE = "rlss/scripts/Car";
/** @brief Wren module of the ball physics. */
inline constexpr std::string_view BALL_SCRIPT_MODULE = "rlss/scripts/Ball";
/** @brief Wren module of the client-side ball visuals. */
inline constexpr std::string_view BALL_VIEW_SCRIPT_MODULE = "rlss/scripts/BallView";
/** @brief Tag of the entities drawing the ball, found by the `BallView` script. */
inline constexpr std::string_view BALL_PART_TAG = "ball_part";
/** @brief Wren module of the client-side car visuals. */
inline constexpr std::string_view CAR_VIEW_SCRIPT_MODULE = "rlss/scripts/CarView";

/** @brief Look of a car: its index in the player palette (`rlss::playerColor`). Replicated. */
struct CarLook {
    /** @brief Palette index. */
    std::uint8_t color = 0;
};

/** @brief State of a car shown to the players, written by the `Car` script. Replicated. */
struct CarStatus {
    /** @brief Boost left, from 0 to 100. */
    float boost = 0.0F;
    /** @brief Boosting right now. */
    bool boosting = false;
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
    /** @brief Boost flame behind the car, hidden when not boosting. */
    ecs::Entity flame = 0;
    /** @brief Background of the boost gauge above the car. */
    ecs::Entity gaugeBack = 0;
    /** @brief Filled part of the boost gauge. */
    ecs::Entity gaugeFill = 0;
};

/** @brief Client: one piece of the ball drawing, placed every frame by the `BallView` script. */
struct BallPart {
    /** @brief Ball entity. */
    ecs::Entity parent = 0;
    /** @brief Offset from the ball center, before rotation. */
    float offsetX = 0.0F;
    /** @brief Offset from the ball center, before rotation. */
    float offsetY = 0.0F;
    /** @brief Rotation of the piece, in radians (added to the ball rotation if it spins). */
    float angle = 0.0F;
    /** @brief True if the piece turns with the ball (its pattern); false for the round body. */
    bool spins = false;
};

}  // namespace rlss::game
