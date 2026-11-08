/**
 * @file Shapes.hpp
 * @brief Flat colored shapes built from the engine's untextured `Renderable` rectangles.
 */

#pragma once

#include "components/Renderable.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace rlss::client {

/** @brief Size in world units of an untextured `Renderable` at scale 1 (see `RenderSystemDefaults.hpp`). */
inline constexpr float PLACEHOLDER_SIZE = 32.0F;

/** @brief Axis-aligned rectangle, in world units. */
struct Rect {
    /** @brief Left edge. */
    float left = 0.0F;
    /** @brief Top edge. */
    float top = 0.0F;
    /** @brief Right edge. */
    float right = 0.0F;
    /** @brief Bottom edge. */
    float bottom = 0.0F;
};

/**
 * @brief Untextured renderable: drawn as a flat colored rectangle.
 * @param color Fill color.
 * @param layer Draw layer.
 * @return The component.
 */
[[nodiscard]] ecs::Renderable makeRenderable(ecs::Rgba color, int layer);

/**
 * @brief Spawns a flat colored rectangle.
 * @param registry World.
 * @param rect Rectangle, in world units.
 * @param color Fill color.
 * @param layer Draw layer.
 * @return The entity.
 */
ecs::Entity spawnRect(ecs::Registry& registry, const Rect& rect, ecs::Rgba color, int layer);

/** @brief Which corners of a rectangle are rounded. */
struct RoundedCorners {
    /** @brief Top-left corner. */
    bool topLeft = true;
    /** @brief Top-right corner. */
    bool topRight = true;
    /** @brief Bottom-left corner. */
    bool bottomLeft = true;
    /** @brief Bottom-right corner. */
    bool bottomRight = true;
};

/**
 * @brief Fills a rectangle with rounded corners, made of rectangles (corners are stacks of thin strips).
 * @param registry World.
 * @param rect Outer rectangle.
 * @param radius Corner radius.
 * @param color Fill color.
 * @param layer Draw layer.
 * @param rounded Corners to round; the others stay square.
 */
void fillRoundedRect(ecs::Registry& registry, const Rect& rect, float radius, ecs::Rgba color, int layer,
                     RoundedCorners rounded = {});

/**
 * @brief Draws a ring of short segments.
 * @param registry World.
 * @param centerX Center, in world units.
 * @param centerY Center, in world units.
 * @param radius Ring radius.
 * @param thickness Line thickness.
 * @param color Line color.
 * @param layer Draw layer.
 */
void strokeCircle(ecs::Registry& registry, float centerX, float centerY, float radius, float thickness, ecs::Rgba color,
                  int layer);

}  // namespace rlss::client
