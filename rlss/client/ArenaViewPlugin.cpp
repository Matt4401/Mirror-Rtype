#include "ArenaViewPlugin.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "Arena.hpp"
#include "Shapes.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "render/RenderPlugin.hpp"

namespace rlss::client {

namespace {

constexpr float WALL = ARENA_WALL;
/** @brief The backdrop extends this far past the arena in every direction. */
constexpr float BACKDROP_MARGIN = 4000.0F;
constexpr float CENTER_CIRCLE_RADIUS = 150.0F;
constexpr float LINE_WIDTH = 6.0F;

/** @brief Behind the walls: a solid structure, not empty space (the camera can show it beside the goals). */
constexpr ecs::Rgba BACKDROP{.r = 72, .g = 80, .b = 98, .a = 255};
constexpr ecs::Rgba WALLS{.r = 175, .g = 186, .b = 204, .a = 255};
constexpr ecs::Rgba GRASS{.r = 64, .g = 150, .b = 70, .a = 255};
constexpr ecs::Rgba INSIDE{.r = 27, .g = 36, .b = 60, .a = 255};
constexpr ecs::Rgba LINES{.r = 255, .g = 255, .b = 255, .a = 45};

constexpr int LAYER_BACKDROP = 0;
constexpr int LAYER_WALLS = 1;
constexpr int LAYER_GRASS = 2;
constexpr int LAYER_INSIDE = 3;
constexpr int LAYER_LINES = 4;
constexpr int LAYER_POSTS = 5;

/** @brief Spacing of the net lines inside the goals. */
constexpr float NET_SPACING = 40.0F;
constexpr float NET_WIDTH = 3.0F;
constexpr float POST_SIZE = 16.0F;
constexpr ecs::Rgba NET{.r = 255, .g = 255, .b = 255, .a = 30};

ecs::Rgba withAlpha(const Rgb color, const std::uint8_t alpha) {
    return ecs::Rgba{.r = color.r, .g = color.g, .b = color.b, .a = alpha};
}

ecs::Rgba darken(const Rgb color) {
    constexpr int DIVISOR = 4;
    constexpr int BASE = 18;
    return ecs::Rgba{
        .r = static_cast<std::uint8_t>(BASE + (color.r / DIVISOR)),
        .g = static_cast<std::uint8_t>(BASE + (color.g / DIVISOR)),
        .b = static_cast<std::uint8_t>(BASE + (color.b / DIVISOR)),
        .a = 255,
    };
}

/**
 * @brief Half the width of a quarter circle of radius `radius` at `offset` from its center (0 past the radius).
 */
float chord(const float radius, const float offset) {
    return offset >= radius ? 0.0F : std::sqrt((radius * radius) - (offset * offset));
}

/**
 * @brief Draws one goal: a frame in the wall color, an inside tinted with the defending team color, a net, the goal
 * line and the posts. The back corners are rounded like the physics (`arena::GOAL_CORNER_RADIUS`).
 * @param wall X of the side wall the goal opens in.
 * @param outward -1 for the left goal, +1 for the right one.
 * @param team Color of the team defending it.
 */
void drawGoal(ecs::Registry& registry, const float wall, const float outward, const Rgb team) {
    const float back = wall + (outward * arena::GOAL_DEPTH);
    const float near = std::min(wall, back);
    const float far = std::max(wall, back);
    const float radius = arena::GOAL_CORNER_RADIUS;
    // Only the back corners are rounded: the left ones for the left goal, the right ones for the right goal.
    const RoundedCorners backCorners{
        .topLeft = outward < 0.0F,
        .topRight = outward > 0.0F,
        .bottomLeft = outward < 0.0F,
        .bottomRight = outward > 0.0F,
    };
    // The frame is the inside grown by the wall thickness: its corners share their centers with the inside ones.
    fillRoundedRect(registry,
                    {
                        .left = near - (outward < 0.0F ? WALL : 0.0F),
                        .top = arena::GOAL_TOP - WALL,
                        .right = far + (outward > 0.0F ? WALL : 0.0F),
                        .bottom = arena::GOAL_BOTTOM + WALL,
                    },
                    radius + WALL, WALLS, LAYER_WALLS, backCorners);
    // Inside, overlapping the wall it cuts through.
    fillRoundedRect(registry,
                    {
                        .left = near - (outward > 0.0F ? WALL : 0.0F),
                        .top = arena::GOAL_TOP,
                        .right = far + (outward < 0.0F ? WALL : 0.0F),
                        .bottom = arena::GOAL_BOTTOM,
                    },
                    radius, darken(team), LAYER_INSIDE, backCorners);

    // The net, cut along the rounded corners. Their centers are `radius` in front of the back, and below the crossbar
    // / above the bottom.
    const float cornerX = back - (outward * radius);
    const auto columns = static_cast<int>((far - near) / NET_SPACING);
    for (int i = 1; i < columns; ++i) {
        const float x = near + (static_cast<float>(i) * NET_SPACING);
        const float past = (x + (NET_WIDTH / 2.0F) - cornerX) * outward;  // how far into the corners
        const float inset = past > 0.0F ? radius - chord(radius, past) : 0.0F;
        if (inset >= radius) {
            continue;
        }
        spawnRect(
            registry,
            {.left = x, .top = arena::GOAL_TOP + inset, .right = x + NET_WIDTH, .bottom = arena::GOAL_BOTTOM - inset},
            NET, LAYER_LINES);
    }
    const auto rows = static_cast<int>(arena::GOAL_HEIGHT / NET_SPACING);
    for (int i = 1; i < rows; ++i) {
        const float y = arena::GOAL_TOP + (static_cast<float>(i) * NET_SPACING);
        const float middle = y + (NET_WIDTH / 2.0F);
        const float fromEdge = std::min(middle - arena::GOAL_TOP, arena::GOAL_BOTTOM - middle);
        const float reach = fromEdge < radius ? chord(radius, radius - fromEdge) : radius;
        const float end = cornerX + (outward * reach);
        spawnRect(registry,
                  {.left = std::min(wall, end), .top = y, .right = std::max(wall, end), .bottom = y + NET_WIDTH}, NET,
                  LAYER_LINES);
    }
    spawnRect(registry,
              {
                  .left = wall - (LINE_WIDTH / 2.0F),
                  .top = arena::GOAL_TOP,
                  .right = wall + (LINE_WIDTH / 2.0F),
                  .bottom = arena::GOAL_BOTTOM,
              },
              withAlpha(team, 170), LAYER_LINES);
    // The bottom post stands where the goal bottom cuts the curved corner, a little inside the arena.
    const float drop = arena::GOAL_BOTTOM - (arena::BOTTOM - arena::CORNER_RADIUS);
    const float postInset = drop > 0.0F ? arena::CORNER_RADIUS - chord(arena::CORNER_RADIUS, drop) : 0.0F;
    const float half = POST_SIZE / 2.0F;
    for (const auto& [x, y] :
         {std::pair{wall, arena::GOAL_TOP}, std::pair{wall - (outward * postInset), arena::GOAL_BOTTOM}}) {
        spawnRect(registry, {.left = x - half, .top = y - half, .right = x + half, .bottom = y + half},
                  withAlpha(team, 255), LAYER_POSTS);
    }
}

}  // namespace

void ArenaViewPlugin::build(engine::PluginBuilder& builder) { builder.require<render::RenderPlugin>(); }

void ArenaViewPlugin::init(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    const Rect inside{.left = arena::LEFT, .top = arena::TOP, .right = arena::RIGHT, .bottom = arena::BOTTOM};
    const Rect outside{
        .left = inside.left - WALL,
        .top = inside.top - WALL,
        .right = inside.right + WALL,
        .bottom = inside.bottom + WALL,
    };

    spawnRect(registry,
              {
                  .left = outside.left - BACKDROP_MARGIN,
                  .top = outside.top - BACKDROP_MARGIN,
                  .right = outside.right + BACKDROP_MARGIN,
                  .bottom = outside.bottom + BACKDROP_MARGIN,
              },
              BACKDROP, LAYER_BACKDROP);
    fillRoundedRect(registry, outside, arena::CORNER_RADIUS + WALL, WALLS, LAYER_WALLS);
    // Grass under the flat part of the floor.
    spawnRect(registry,
              {
                  .left = inside.left + arena::CORNER_RADIUS,
                  .top = inside.bottom - 2.0F,
                  .right = inside.right - arena::CORNER_RADIUS,
                  .bottom = inside.bottom + (WALL / 2.0F),
              },
              GRASS, LAYER_GRASS);
    fillRoundedRect(registry, inside, arena::CORNER_RADIUS, INSIDE, LAYER_INSIDE);
    // The blue team defends the left goal, the orange team the right one.
    drawGoal(registry, arena::LEFT, -1.0F, BLUE_TEAM);
    drawGoal(registry, arena::RIGHT, 1.0F, ORANGE_TEAM);

    const float centerX = (inside.left + inside.right) / 2.0F;
    const float centerY = (inside.top + inside.bottom) / 2.0F;
    spawnRect(registry,
              {
                  .left = centerX - (LINE_WIDTH / 2.0F),
                  .top = inside.top,
                  .right = centerX + (LINE_WIDTH / 2.0F),
                  .bottom = inside.bottom,
              },
              LINES, LAYER_LINES);
    strokeCircle(registry, centerX, centerY, CENTER_CIRCLE_RADIUS, LINE_WIDTH, LINES, LAYER_LINES);
}

}  // namespace rlss::client
