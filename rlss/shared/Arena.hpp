/**
 * @file Arena.hpp
 * @brief Dimensions of the RLSS arena and the player palette, shared by the server (physics) and the client (drawing).
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rlss {

/**
 * @brief The arena is a rectangle with rounded corners, seen from the side (y points down).
 *
 * Units are Rocket League's "unreal units" (uu): a car is 118 uu long, gravity is 650 uu/s² in Rocket League. The ball
 * is a little smaller than Rocket League's (182.5 uu wide). The arena is about 24 car lengths wide: cars and ball keep
 * their Rocket League size, the camera follows the action instead of showing the whole arena.
 */
namespace arena {

/** @brief Left wall, in uu. */
inline constexpr float LEFT = 0.0F;
/** @brief Ceiling, in uu. */
inline constexpr float TOP = 0.0F;
/** @brief Inner width (wall to wall), in uu. */
inline constexpr float WIDTH = 2880.0F;
/** @brief Inner height (floor to ceiling), in uu. */
inline constexpr float HEIGHT = 1536.0F;
/** @brief Right wall, in uu. */
inline constexpr float RIGHT = LEFT + WIDTH;
/** @brief Floor, in uu. */
inline constexpr float BOTTOM = TOP + HEIGHT;
/** @brief Radius of the four curved corners joining the walls to the floor and the ceiling, in uu. */
inline constexpr float CORNER_RADIUS = 420.0F;

/**
 * @brief How far the bottom of the goal mouths is below the top of the curved corners, in uu. Like in Sideswipe, the
 * goals open in the side walls a little above the floor: the curved corner is a ramp that ends at the goal. The goal
 * bottom cuts the top of that ramp, so the bottom post stands on the curve, a few uu inside the arena.
 */
inline constexpr float GOAL_DROP = 60.0F;
/** @brief Bottom of the goal mouths, in uu. */
inline constexpr float GOAL_BOTTOM = BOTTOM - CORNER_RADIUS + GOAL_DROP;
/** @brief Height of the goal mouths (about 3.4 ball diameters), in uu. */
inline constexpr float GOAL_HEIGHT = 540.0F;
/** @brief Top of the goal mouths (the crossbar), in uu. */
inline constexpr float GOAL_TOP = GOAL_BOTTOM - GOAL_HEIGHT;
/** @brief Depth of the goals behind the side walls: the whole ball fits behind the goal line. */
inline constexpr float GOAL_DEPTH = 240.0F;
/**
 * @brief Radius of the two back corners of each goal (where the back meets the crossbar and the bottom), in uu: a bit
 * more than the ball, so that it rolls around them. At most half the goal height and the goal depth.
 */
inline constexpr float GOAL_CORNER_RADIUS = 120.0F;

static_assert(GOAL_DROP >= 0.0F && GOAL_DROP < CORNER_RADIUS, "the goal bottom must cut the ramp, not go below it");
static_assert(GOAL_TOP >= TOP + CORNER_RADIUS, "the crossbar must stay below the upper curved corner");
static_assert(GOAL_CORNER_RADIUS <= GOAL_DEPTH && GOAL_CORNER_RADIUS * 2.0F <= GOAL_HEIGHT, "goal corners too round");

}  // namespace arena

/** @brief Radius of the ball, in uu: the same as `BallTuning.RADIUS` in the Wren physics. */
inline constexpr float BALL_RADIUS = 80.0F;

/** @brief An 8-bit RGB color, independent of any graphics library. */
struct Rgb {
    /** @brief Red component. */
    std::uint8_t r = 0;
    /** @brief Green component. */
    std::uint8_t g = 0;
    /** @brief Blue component. */
    std::uint8_t b = 0;
};

/**
 * @brief One color per player slot. Even slots play on the left (blue team, cool colors), odd slots on the right
 * (orange team, warm colors). Slots past the end wrap around.
 */
inline constexpr std::array<Rgb, 8> PLAYER_COLORS{
    {
        // clang-format off
    {.r = 30, .g = 120, .b = 255},  // blue
    {.r = 255, .g = 130, .b = 20},  // orange
    {.r = 40, .g = 215, .b = 215},  // cyan
    {.r = 230, .g = 50, .b = 50},   // red
    {.r = 170, .g = 80, .b = 230},  // purple
    {.r = 245, .g = 215, .b = 40},  // yellow
    {.r = 60, .g = 200, .b = 90},   // green
    {.r = 245, .g = 90, .b = 170},  // pink
},};

/**
 * @brief Color of a palette index.
 * @param index Palette index (any value: wraps around).
 * @return The color.
 */
[[nodiscard]] constexpr Rgb playerColor(const std::size_t index) noexcept {
    return PLAYER_COLORS.at(index % PLAYER_COLORS.size());
}

/** @brief Color of the blue team (left side, defends the left goal). */
inline constexpr Rgb BLUE_TEAM{.r = 30, .g = 120, .b = 255};
/** @brief Color of the orange team (right side, defends the right goal). */
inline constexpr Rgb ORANGE_TEAM{.r = 255, .g = 130, .b = 20};

}  // namespace rlss
