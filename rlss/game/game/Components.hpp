/**
 * @file Components.hpp
 * @brief RLSS game components and constants shared by the server and the client.
 */

#pragma once

#include <cstdint>
#include <string_view>

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
/** @brief Wren module of the client-side scoreboard. */
inline constexpr std::string_view SCOREBOARD_SCRIPT_MODULE = "rlss/scripts/ScoreboardView";
/** @brief Tag of the squares of the scoreboard, found by the `ScoreboardView` script. */
inline constexpr std::string_view SCORE_CELL_TAG = "score_cell";
/** @brief Wren module of the client camera. */
inline constexpr std::string_view CAMERA_SCRIPT_MODULE = "rlss/scripts/CameraDirector";
/** @brief Tag of the camera entity, found by the scripts that draw on screen (scoreboard). */
inline constexpr std::string_view CAMERA_TAG = "main_camera";
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
    /** @brief Set by the server on a new car: its script moves it to its kickoff spot. Not replicated. */
    bool placeAtKickoff = false;
};

/**
 * @brief Score and phase of the match. Server: written by the `Match` script on the arena entity, sent to the clients
 * in `MatchStatus` messages. Client: written from those messages, read by the `ScoreboardView` script.
 */
struct MatchState {
    /** @brief Goals of the blue team (left side, defends the left goal). */
    int blueScore = 0;
    /** @brief Goals of the orange team (right side, defends the right goal). */
    int orangeScore = 0;
    /** @brief `protocol::MatchPhase` value: 0 playing, 1 goal celebration, 2 kickoff countdown. */
    int phase = 0;
    /** @brief Kickoff countdown shown to the players (3, 2, 1), 0 outside the kickoff. */
    int countdown = 0;
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
    /** @brief Top of both goal mouths (the crossbar). */
    float goalTop = 0.0F;
    /** @brief Bottom of both goal mouths. */
    float goalBottom = 0.0F;
    /** @brief Depth of the goals behind the side walls. */
    float goalDepth = 0.0F;
};

/** @brief Client: entities drawing the parts of a car, placed every frame by the `CarView` script. */
struct CarParts {
    /** @brief Chassis, in the player color. */
    std::uint32_t body = 0;
    /** @brief Cabin, on top of the chassis. */
    std::uint32_t cabin = 0;
    /** @brief Rear wheel. */
    std::uint32_t rearWheel = 0;
    /** @brief Front wheel. */
    std::uint32_t frontWheel = 0;
    /** @brief Headlight, shows where the car is facing. */
    std::uint32_t headlight = 0;
    /** @brief Boost flame behind the car, hidden when not boosting. */
    std::uint32_t flame = 0;
    /** @brief Background of the boost gauge above the car. */
    std::uint32_t gaugeBack = 0;
    /** @brief Filled part of the boost gauge. */
    std::uint32_t gaugeFill = 0;
};

/** @brief Client: one piece of the ball drawing, placed every frame by the `BallView` script. */
struct BallPart {
    /** @brief Ball entity. */
    std::uint32_t parent = 0;
    /** @brief Offset from the ball center, before rotation. */
    float offsetX = 0.0F;
    /** @brief Offset from the ball center, before rotation. */
    float offsetY = 0.0F;
    /** @brief Rotation of the piece, in radians (added to the ball rotation if it spins). */
    float angle = 0.0F;
    /** @brief True if the piece turns with the ball (its pattern); false for the round body. */
    bool spins = false;
};

/**
 * @brief Client: one square of the scoreboard, lit or hidden by the `ScoreboardView` script.
 *
 * Placed on screen, not in the world: the script converts its screen position through the camera every frame, so the
 * scoreboard stays put while the camera moves.
 */
struct ScoreCell {
    /** @brief What it belongs to: 0 blue score, 1 orange score, 2 kickoff countdown, 3 "GOAL" banner, 4 always shown.
     */
    int display = 0;
    /** @brief Character of the display, from the left. */
    int slot = 0;
    /** @brief Row in the 3 x 5 glyph, from the top. */
    int row = 0;
    /** @brief Column in the 3 x 5 glyph, from the left. */
    int column = 0;
    /** @brief Center on screen, in pixels from the left. */
    float screenX = 0.0F;
    /** @brief Center on screen, in pixels from the top. */
    float screenY = 0.0F;
    /** @brief Width when lit, in pixels. */
    float width = 0.0F;
    /** @brief Height when lit, in pixels. */
    float height = 0.0F;
};

/** @brief "No entity" in the entity fields read by scripts (0 is a valid entity). */
inline constexpr int NO_ENTITY = -1;

/**
 * @brief Client: what the `CameraDirector` script frames, kept up to date by `CameraPlugin` on the camera entity.
 */
struct CameraRig {
    /** @brief Local player's car entity, `NO_ENTITY` while unknown. */
    int car = NO_ENTITY;
    /** @brief Ball entity, `NO_ENTITY` while unknown. */
    int ball = NO_ENTITY;
    /** @brief `protocol::MatchPhase` value: 0 playing, 1 goal celebration, 2 kickoff countdown. */
    int phase = 0;
    /** @brief Window size, in pixels. */
    float viewWidth = 0.0F;
    /** @brief Window size, in pixels. */
    float viewHeight = 0.0F;
    /** @brief The camera never shows anything left of this (outer side of the left goal). */
    float boundsLeft = 0.0F;
    /** @brief The camera never shows anything above this (outer side of the ceiling wall). */
    float boundsTop = 0.0F;
    /** @brief The camera never shows anything right of this. */
    float boundsRight = 0.0F;
    /** @brief The camera never shows anything below this (outer side of the floor). */
    float boundsBottom = 0.0F;
};

}  // namespace rlss::game
