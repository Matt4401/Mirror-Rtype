/**
 * @file UiCanvas.hpp
 * @brief Flat rectangles and block text placed on screen, drawn through the game camera.
 */

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "components/CameraComponent.hpp"
#include "components/Renderable.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace rlss::client {

/** @brief Rectangle on screen, in pixels from the top-left corner of the window. */
struct ScreenRect {
    /** @brief Left edge. */
    float x = 0.0F;
    /** @brief Top edge. */
    float y = 0.0F;
    /** @brief Width. */
    float width = 0.0F;
    /** @brief Height. */
    float height = 0.0F;

    /**
     * @brief Whether a point is inside.
     * @param px Point, in pixels.
     * @param py Point, in pixels.
     * @return True if inside (edges included).
     */
    [[nodiscard]] bool contains(const float px, const float py) const noexcept {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }
};

/** @brief Where a text is anchored, horizontally. */
enum class Align : std::uint8_t {
    /** @brief `x` is the left edge of the text. */
    Left,
    /** @brief `x` is the middle of the text. */
    Center,
    /** @brief `x` is the right edge of the text. */
    Right,
};

/**
 * @brief Owns the entities of a screen-space layer: menus, buttons, labels.
 *
 * The engine only draws in the world, through the camera; `place()` moves and scales every element each frame so that
 * it stays put on screen whatever the camera does. Elements are untextured `Renderable` rectangles; a text is one
 * rectangle per run of lit squares of its `font` glyphs.
 */
class UiCanvas {
  public:
    /**
     * @brief Spawns a rectangle.
     * @param registry World.
     * @param rect Place on screen.
     * @param color Fill color.
     * @param layer Draw layer.
     * @return The entity, to change its tint later.
     */
    ecs::Entity rect(ecs::Registry& registry, const ScreenRect& rect, ecs::Rgba color, int layer);

    /**
     * @brief Spawns a text in the block font.
     * @param registry World.
     * @param text Text (see `font::forEachRun` for the characters drawn).
     * @param x Anchor, in pixels; see `align`.
     * @param top Top edge, in pixels.
     * @param square Side of one square of a glyph, in pixels (a glyph is 5 x 7 squares).
     * @param color Text color.
     * @param layer Draw layer.
     * @param align What `x` is.
     */
    void text(ecs::Registry& registry, std::string_view text, float x, float top, float square, ecs::Rgba color,
              int layer, Align align = Align::Center);

    /**
     * @brief Width of a text drawn by `text()`.
     * @param text Text.
     * @param square Side of one square, in pixels.
     * @return Width in pixels.
     */
    [[nodiscard]] static float textWidth(std::string_view text, float square) noexcept;

    /**
     * @brief Kills every element.
     * @param registry World.
     */
    void clear(ecs::Registry& registry);

    /**
     * @brief Moves every element to its place on screen, seen through a camera. Call it once the camera is final for
     * the frame.
     * @param registry World.
     * @param camera Camera the world is drawn through.
     */
    void place(ecs::Registry& registry, const ecs::CameraComponent& camera) const;

  private:
    /** @brief An element and its place on screen. */
    struct Item {
        /** @brief Entity. */
        ecs::Entity entity = 0;
        /** @brief Place on screen. */
        ScreenRect rect;
    };

    /** @brief Elements, in spawn order. */
    std::vector<Item> _items;
};

}  // namespace rlss::client
