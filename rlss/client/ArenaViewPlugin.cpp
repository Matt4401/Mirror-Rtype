#include "ArenaViewPlugin.hpp"

#include <algorithm>
#include <cstdint>

#include "Arena.hpp"
#include "Shapes.hpp"
#include "components/CameraComponent.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "render/RenderPlugin.hpp"

namespace rlss::client {

namespace {

/** @brief Thickness of the walls drawn around the playing area. */
constexpr float WALL = 40.0F;
/** @brief Space kept around the arena in the window, in pixels. */
constexpr float SCREEN_MARGIN = 24.0F;
/** @brief The backdrop extends this far past the arena in every direction. */
constexpr float BACKDROP_MARGIN = 4000.0F;
constexpr float CENTER_CIRCLE_RADIUS = 150.0F;
constexpr float LINE_WIDTH = 6.0F;

constexpr ecs::Rgba BACKDROP{.r = 12, .g = 15, .b = 24, .a = 255};
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
 * @brief Draws one goal: a frame in the wall color, an inside tinted with the defending team color, a net, the goal
 * line and the posts.
 * @param wall X of the side wall the goal opens in.
 * @param outward -1 for the left goal, +1 for the right one.
 * @param team Color of the team defending it.
 */
void drawGoal(ecs::Registry& registry, const float wall, const float outward, const Rgb team) {
    const float back = wall + (outward * arena::GOAL_DEPTH);
    const float near = std::min(wall, back);
    const float far = std::max(wall, back);
    spawnRect(registry,
              {
                  .left = near - (outward < 0.0F ? WALL : 0.0F),
                  .top = arena::GOAL_TOP - WALL,
                  .right = far + (outward > 0.0F ? WALL : 0.0F),
                  .bottom = arena::GOAL_BOTTOM + WALL,
              },
              WALLS, LAYER_WALLS);
    // Inside, overlapping the wall it cuts through.
    spawnRect(registry,
              {
                  .left = near - (outward > 0.0F ? WALL : 0.0F),
                  .top = arena::GOAL_TOP,
                  .right = far + (outward < 0.0F ? WALL : 0.0F),
                  .bottom = arena::GOAL_BOTTOM,
              },
              darken(team), LAYER_INSIDE);
    const auto columns = static_cast<int>((far - near) / NET_SPACING);
    for (int i = 1; i < columns; ++i) {
        const float x = near + (static_cast<float>(i) * NET_SPACING);
        spawnRect(registry, {.left = x, .top = arena::GOAL_TOP, .right = x + NET_WIDTH, .bottom = arena::GOAL_BOTTOM},
                  NET, LAYER_LINES);
    }
    const auto rows = static_cast<int>(arena::GOAL_HEIGHT / NET_SPACING);
    for (int i = 1; i < rows; ++i) {
        const float y = arena::GOAL_TOP + (static_cast<float>(i) * NET_SPACING);
        spawnRect(registry, {.left = near, .top = y, .right = far, .bottom = y + NET_WIDTH}, NET, LAYER_LINES);
    }
    spawnRect(registry,
              {
                  .left = wall - (LINE_WIDTH / 2.0F),
                  .top = arena::GOAL_TOP,
                  .right = wall + (LINE_WIDTH / 2.0F),
                  .bottom = arena::GOAL_BOTTOM,
              },
              withAlpha(team, 170), LAYER_LINES);
    for (const float y : {arena::GOAL_TOP, arena::GOAL_BOTTOM}) {
        const float half = POST_SIZE / 2.0F;
        spawnRect(registry, {.left = wall - half, .top = y - half, .right = wall + half, .bottom = y + half},
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

    const auto& window = engine.service<engine::IWindowService>();
    const auto width = static_cast<float>(window.width());
    const auto height = static_cast<float>(window.height());
    const float fullWidth = outside.right - outside.left + (2.0F * arena::GOAL_DEPTH);
    const float zoom = std::min((width - (2.0F * SCREEN_MARGIN)) / fullWidth,
                                (height - (2.0F * SCREEN_MARGIN)) / (outside.bottom - outside.top));
    const ecs::Entity camera = registry.spawnEntity();
    registry.addComponent(camera, ecs::CameraComponent{
                                      .targetX = centerX,
                                      .targetY = centerY,
                                      .offsetX = width / 2.0F,
                                      .offsetY = height / 2.0F,
                                      .zoom = zoom,
                                  });
}

}  // namespace rlss::client
