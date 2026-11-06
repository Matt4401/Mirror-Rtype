#include "ArenaViewPlugin.hpp"

#include <algorithm>

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
    const float zoom = std::min((width - (2.0F * SCREEN_MARGIN)) / (outside.right - outside.left),
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
