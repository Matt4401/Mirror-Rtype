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

/** @brief Horizontal axis action in [-1, 1]: drive on the ground; in the air, with `ACTION_PITCH`, where the nose aims.
 */
inline constexpr std::string_view ACTION_STEER = "steer";
/** @brief Vertical axis action in [-1, 1], up positive: in the air, with `ACTION_STEER`, where the nose aims. */
inline constexpr std::string_view ACTION_PITCH = "pitch";
/** @brief Jump button action. */
inline constexpr std::string_view ACTION_JUMP = "jump";
/** @brief Boost button action. */
inline constexpr std::string_view ACTION_BOOST = "boost";
/** @brief Id of `ACTION_STEER`. */
inline constexpr input::ActionId STEER_ID = input::makeActionId(ACTION_STEER);
/** @brief Id of `ACTION_PITCH`. */
inline constexpr input::ActionId PITCH_ID = input::makeActionId(ACTION_PITCH);
/** @brief Id of `ACTION_JUMP`. */
inline constexpr input::ActionId JUMP_ID = input::makeActionId(ACTION_JUMP);
/** @brief Id of `ACTION_BOOST`. */
inline constexpr input::ActionId BOOST_ID = input::makeActionId(ACTION_BOOST);

/** @brief Tag (`ecs::Tag::name`) of the entity carrying the `ArenaShape`, found by the Wren scripts. */
inline constexpr std::string_view ARENA_TAG = "arena";

/** @brief Wren module (file path without `.wren`, relative to the script root) of the match simulation. */
inline constexpr std::string_view MATCH_SCRIPT_MODULE = "rlss/scripts/Match";
/** @brief Wren module of the car physics. */
inline constexpr std::string_view CAR_SCRIPT_MODULE = "rlss/scripts/Car";
/** @brief Wren module of the ball physics. */
inline constexpr std::string_view BALL_SCRIPT_MODULE = "rlss/scripts/Ball";
/** @brief Wren module of the client-side ball visuals. */
inline constexpr std::string_view BALL_VIEW_SCRIPT_MODULE = "rlss/scripts/BallView";
/** @brief Tag of the entities drawing the ball, found by the `BallView` script. */
inline constexpr std::string_view BALL_PART_TAG = "ball_part";
/** @brief Wren module of the client camera. */
inline constexpr std::string_view CAMERA_SCRIPT_MODULE = "rlss/scripts/CameraDirector";
/** @brief Tag of the camera entity. */
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
    /** @brief Flip regained on the ball with the wheels, not used yet: the car flashes white. Replicated. */
    bool flipReset = false;
};

/**
 * @brief Speed (uu/s) heard at full strength in the sound cues: a hit, a bounce or an impact this fast or faster is
 * sent as the strongest level (`protocol::SOUND_CUE_MAX`).
 */
inline constexpr float SOUND_CUE_FULL_SPEED = 2400.0F;

/** @brief Values of `CarCues::jumpKind`, as in `rlss/scripts/Car.wren`. */
enum class JumpKind : std::uint8_t {
    /** @brief First jump, off a surface (or rolling back onto the wheels). */
    Jump = 0,
    /** @brief Second jump without direction, or jumping off the ball (purple shot). */
    DoubleJump = 1,
    /** @brief Second jump with a direction: a flip. */
    Dodge = 2,
};

/**
 * @brief Server: what a car did that makes a sound, written by the `Car` script, replicated as sound cues.
 *
 * The counters only grow (the wire keeps their low 4 bits): a client plays a sound when one changes, so a lost snapshot
 * delays the sound instead of losing it.
 */
struct CarCues {
    /** @brief Jumps of any kind made so far. */
    int jumps = 0;
    /** @brief `JumpKind` of the last jump. */
    int jumpKind = 0;
    /** @brief Hard contacts with the arena so far: landings and the body hitting a wall. */
    int impacts = 0;
    /** @brief Speed into the surface of the last impact, in uu/s. */
    float impactSpeed = 0.0F;
};

/**
 * @brief Server: what the ball did that makes a sound, written by the `Ball` script, replicated as sound cues. Counters
 * work like `CarCues`.
 */
struct BallCues {
    /** @brief Hits by a car so far. */
    int hits = 0;
    /** @brief Speed of the last hit along the contact, in uu/s. */
    float hitSpeed = 0.0F;
    /** @brief Bounces off the arena so far. */
    int bounces = 0;
    /** @brief Speed into the wall of the last bounce, in uu/s. */
    float bounceSpeed = 0.0F;
};

/** @brief Values of `ShotGlow::shot`, as in `rlss/scripts/Shots.wren`. */
enum class Shot : std::uint8_t {
    /** @brief No glow. */
    None = 0,
    /** @brief Power shot: flipping into the ball with the nose. */
    Red = 1,
    /** @brief Underside shot: jumping off the ball with the wheels. */
    Purple = 2,
    /** @brief Back shot: flipping the rear into the ball, a lob. */
    Gold = 3,
};

/** @brief Ball: color of the last shot, written by the `Ball` script, replicated, shown by the `BallView` script. */
struct ShotGlow {
    /** @brief `Shot` value. */
    int shot = 0;
};

/**
 * @brief Score and phase of the match. Server: written by the `Match` script on the arena entity, sent to the clients
 * in `MatchStatus` messages. Client: written from those messages by `HudPlugin`, read by the sounds.
 */
struct MatchState {
    /** @brief Goals of the blue team (left side, defends the left goal). */
    int blueScore = 0;
    /** @brief Goals of the orange team (right side, defends the right goal). */
    int orangeScore = 0;
    /**
     * @brief `protocol::MatchPhase` value: 0 playing, 1 goal celebration, 2 kickoff countdown, 3 ended. On the client,
     * `NO_MATCH` while the player is in the menu.
     */
    int phase = 0;
    /** @brief Kickoff countdown shown to the players (3, 2, 1), 0 outside the kickoff. */
    int countdown = 0;
    /** @brief Match clock in whole seconds: time left, or overtime played when `overtime` is set. */
    int clock = 0;
    /** @brief True in overtime: the next goal wins. */
    bool overtime = false;
};

/** @brief `MatchState::phase` of the client HUD while no match is joined: nothing is shown. */
inline constexpr int NO_MATCH = -1;

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
    /** @brief Radius of the back corners of the goals (0: sharp corners). */
    float goalCornerRadius = 0.0F;
};

/** @brief Client: entities drawing the parts of a car, placed every frame by the `CarView` script. */
struct CarParts {
    /** @brief Body, in the player color. */
    ecs::Entity body = 0;
    /** @brief Glass, trims and lights, drawn over the body. */
    ecs::Entity cabin = 0;
    /** @brief Rear wheel. */
    ecs::Entity rearWheel = 0;
    /** @brief Front wheel. */
    ecs::Entity frontWheel = 0;
    /** @brief Headlight glow, shows where the car is facing. */
    ecs::Entity headlight = 0;
    /** @brief Boost flame behind the car, hidden when not boosting. */
    ecs::Entity flame = 0;
    /** @brief Background of the boost gauge above the car. */
    ecs::Entity gaugeBack = 0;
    /** @brief Filled part of the boost gauge. */
    ecs::Entity gaugeFill = 0;
    /** @brief White body overlay, blinking after a flip reset. */
    ecs::Entity flash = 0;
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
    /** @brief `Shot` this piece shows (a colored halo), or 0 for a piece always shown. */
    int glow = 0;
};

/**
 * @brief "No entity" in the entity fields read by scripts (0 is a valid entity). Those fields are 64-bit signed: an
 * `ecs::Entity` (generation and index, 52 bits) fits, and so does -1.
 */
inline constexpr std::int64_t NO_ENTITY = -1;

/**
 * @brief Client: what the `CameraDirector` script frames, kept up to date by `CameraPlugin` on the camera entity.
 */
struct CameraRig {
    /** @brief Local player's car entity, `NO_ENTITY` while unknown. */
    std::int64_t car = NO_ENTITY;
    /** @brief Ball entity, `NO_ENTITY` while unknown. */
    std::int64_t ball = NO_ENTITY;
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
