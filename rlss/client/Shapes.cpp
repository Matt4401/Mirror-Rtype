#include "Shapes.hpp"

#include <array>
#include <cmath>
#include <numbers>

#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace rlss::client {

namespace {

/** @brief Strips per rounded corner: thin enough to look round once zoomed out. */
constexpr int CORNER_STRIPS = 48;
/** @brief Segments of a ring. */
constexpr int CIRCLE_SEGMENTS = 64;

}  // namespace

ecs::Renderable makeRenderable(const ecs::Rgba color, const int layer) {
    ecs::Renderable renderable;
    renderable.tint = color;
    renderable.layer = layer;
    return renderable;
}

ecs::Entity spawnRect(ecs::Registry& registry, const Rect& rect, const ecs::Rgba color, const int layer) {
    const ecs::Entity entity = registry.spawnEntity();
    registry.addComponent(entity, ecs::Transform{
                                      .x = (rect.left + rect.right) / 2.0F,
                                      .y = (rect.top + rect.bottom) / 2.0F,
                                      .scaleX = (rect.right - rect.left) / PLACEHOLDER_SIZE,
                                      .scaleY = (rect.bottom - rect.top) / PLACEHOLDER_SIZE,
                                  });
    registry.addComponent(entity, makeRenderable(color, layer));
    return entity;
}

void fillRoundedRect(ecs::Registry& registry, const Rect& rect, const float radius, const ecs::Rgba color,
                     const int layer, const RoundedCorners rounded) {
    const float r = radius;
    spawnRect(registry, {.left = rect.left, .top = rect.top + r, .right = rect.right, .bottom = rect.bottom - r}, color,
              layer);
    spawnRect(registry, {.left = rect.left + r, .top = rect.top, .right = rect.right - r, .bottom = rect.top + r},
              color, layer);
    spawnRect(registry, {.left = rect.left + r, .top = rect.bottom - r, .right = rect.right - r, .bottom = rect.bottom},
              color, layer);

    struct Corner {
        float centerX;
        float centerY;
        float sideX;
        float sideY;
        bool round;
    };
    const std::array<Corner, 4> corners{
        {
            {
                .centerX = rect.left + r,
                .centerY = rect.top + r,
                .sideX = -1.0F,
                .sideY = -1.0F,
                .round = rounded.topLeft,
            },
            {
                .centerX = rect.right - r,
                .centerY = rect.top + r,
                .sideX = 1.0F,
                .sideY = -1.0F,
                .round = rounded.topRight,
            },
            {
                .centerX = rect.left + r,
                .centerY = rect.bottom - r,
                .sideX = -1.0F,
                .sideY = 1.0F,
                .round = rounded.bottomLeft,
            },
            {
                .centerX = rect.right - r,
                .centerY = rect.bottom - r,
                .sideX = 1.0F,
                .sideY = 1.0F,
                .round = rounded.bottomRight,
            },
        },
    };
    const float strip = r / static_cast<float>(CORNER_STRIPS);
    for (const Corner& corner : corners) {
        if (!corner.round) {
            // A square corner: one r x r square.
            const float x1 = corner.centerX + (corner.sideX * r);
            const float y1 = corner.centerY + (corner.sideY * r);
            spawnRect(registry,
                      {
                          .left = std::fmin(corner.centerX, x1),
                          .top = std::fmin(corner.centerY, y1),
                          .right = std::fmax(corner.centerX, x1),
                          .bottom = std::fmax(corner.centerY, y1),
                      },
                      color, layer);
            continue;
        }
        for (int i = 0; i < CORNER_STRIPS; ++i) {
            const float near = static_cast<float>(i) * strip;
            const float middle = near + (strip / 2.0F);
            const float halfWidth = std::sqrt((r * r) - (middle * middle));
            const float x0 = corner.centerX;
            const float x1 = corner.centerX + (corner.sideX * halfWidth);
            const float y0 = corner.centerY + (corner.sideY * near);
            const float y1 = corner.centerY + (corner.sideY * (near + strip));
            spawnRect(registry,
                      {
                          .left = std::fmin(x0, x1),
                          .top = std::fmin(y0, y1),
                          .right = std::fmax(x0, x1),
                          .bottom = std::fmax(y0, y1),
                      },
                      color, layer);
        }
    }
}

void strokeCircle(ecs::Registry& registry, const float centerX, const float centerY, const float radius,
                  const float thickness, const ecs::Rgba color, const int layer) {
    const float step = 2.0F * std::numbers::pi_v<float> / static_cast<float>(CIRCLE_SEGMENTS);
    const float length = radius * step * 1.1F;
    for (int i = 0; i < CIRCLE_SEGMENTS; ++i) {
        const float angle = static_cast<float>(i) * step;
        const ecs::Entity entity = registry.spawnEntity();
        registry.addComponent(entity, ecs::Transform{
                                          .x = centerX + (radius * std::cos(angle)),
                                          .y = centerY + (radius * std::sin(angle)),
                                          .rotation = angle + (std::numbers::pi_v<float> / 2.0F),
                                          .scaleX = length / PLACEHOLDER_SIZE,
                                          .scaleY = thickness / PLACEHOLDER_SIZE,
                                      });
        registry.addComponent(entity, makeRenderable(color, layer));
    }
}

}  // namespace rlss::client
