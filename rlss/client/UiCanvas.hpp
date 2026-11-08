/**
 * @file UiCanvas.hpp
 * @brief Screen-space layer of rectangles, pictures and text, drawn on top of the world every frame.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"

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
 * @brief A font and the service that measures and draws it. Texts are sized by the height of their capital letters,
 * so that layouts do not depend on the font's line spacing.
 */
class Typeface {
  public:
    /** @brief No font: measures nothing, draws nothing (until `bind`). */
    Typeface() = default;

    /**
     * @brief Uses a font.
     * @param draw Service drawing it; must outlive the typeface.
     * @param font Font loaded by `draw` (`engine::NO_FONT`: the backend's default font).
     */
    Typeface(engine::IScreenDrawService& draw, engine::FontId font) noexcept : _draw(&draw), _font(font) {}

    /**
     * @brief Width of a text.
     * @param text Text.
     * @param capHeight Height of its capital letters, in pixels.
     * @return Width in pixels, 0 without a service.
     */
    [[nodiscard]] float width(std::string_view text, float capHeight) const;

    /**
     * @brief Queues a text.
     * @param text Text.
     * @param x Anchor, in pixels; see `align`.
     * @param capTop Top of the capital letters, in pixels.
     * @param capHeight Height of the capital letters, in pixels.
     * @param color Color.
     * @param align What `x` is.
     */
    void draw(std::string_view text, float x, float capTop, float capHeight, ecs::Rgba color, Align align) const;

    /** @brief The drawing service, or null. */
    [[nodiscard]] engine::IScreenDrawService* service() const noexcept { return _draw; }

  private:
    /** @brief Service, or null. */
    engine::IScreenDrawService* _draw = nullptr;
    /** @brief Font. */
    engine::FontId _font = engine::NO_FONT;
};

/** @brief Index of an element of a `UiCanvas`, to change its color later. */
using UiHandle = std::size_t;

/**
 * @brief Retained list of screen-space elements (menus, labels), drawn through `engine::IScreenDrawService`.
 *
 * Build the elements when the screen changes, then call `draw()` every frame: they are drawn in the order they were
 * added, on top of the world.
 */
class UiCanvas {
  public:
    /**
     * @brief Adds a filled rectangle.
     * @param rect Place on screen.
     * @param color Fill color.
     * @return Its handle.
     */
    UiHandle rect(const ScreenRect& rect, ecs::Rgba color);

    /**
     * @brief Adds a rectangle outline (drawn inside the rectangle).
     * @param rect Place on screen.
     * @param thickness Line thickness, in pixels.
     * @param color Line color.
     * @return Its handle.
     */
    UiHandle outline(const ScreenRect& rect, float thickness, ecs::Rgba color);

    /**
     * @brief Adds a picture.
     * @param texture Texture (`ecs::NO_TEXTURE` draws nothing).
     * @param rect Place on screen, the whole texture stretched over it.
     * @param tint Color multiplied with the texture.
     * @return Its handle.
     */
    UiHandle image(ecs::TextureId texture, const ScreenRect& rect, ecs::Rgba tint);

    /**
     * @brief Adds a text.
     * @param text Text.
     * @param x Anchor, in pixels; see `align`.
     * @param capTop Top of the capital letters, in pixels.
     * @param capHeight Height of the capital letters, in pixels.
     * @param color Text color.
     * @param align What `x` is.
     * @return Its handle.
     */
    UiHandle text(std::string_view text, float x, float capTop, float capHeight, ecs::Rgba color,
                  Align align = Align::Center);

    /**
     * @brief Changes the color (or tint) of an element.
     * @param handle Element; an unknown one is ignored.
     * @param color New color.
     */
    void setColor(UiHandle handle, ecs::Rgba color);

    /** @brief Removes every element. */
    void clear() noexcept { _items.clear(); }

    /**
     * @brief Queues every element, in the order they were added.
     * @param typeface Font of the texts, and the service everything is drawn with (nothing without one).
     * @param shiftX Moves every element right by this many pixels (a label following something in the world).
     * @param shiftY Moves every element down by this many pixels.
     */
    void draw(const Typeface& typeface, float shiftX = 0.0F, float shiftY = 0.0F) const;

    /** @brief True when the canvas has no element. */
    [[nodiscard]] bool empty() const noexcept { return _items.empty(); }

  private:
    /** @brief Filled rectangle. */
    struct Fill {
        /** @brief Place. */
        ScreenRect rect;
    };
    /** @brief Rectangle outline. */
    struct Outline {
        /** @brief Place. */
        ScreenRect rect;
        /** @brief Line thickness. */
        float thickness = 1.0F;
    };
    /** @brief Picture. */
    struct Image {
        /** @brief Texture. */
        ecs::TextureId texture = ecs::NO_TEXTURE;
        /** @brief Place. */
        ScreenRect rect;
    };
    /** @brief Text. */
    struct Text {
        /** @brief Text. */
        std::string text;
        /** @brief Anchor. */
        float x = 0.0F;
        /** @brief Top of the capitals. */
        float capTop = 0.0F;
        /** @brief Height of the capitals. */
        float capHeight = 0.0F;
        /** @brief Anchoring. */
        Align align = Align::Center;
    };
    /** @brief An element and its color. */
    struct Item {
        /** @brief What it is. */
        std::variant<Fill, Outline, Image, Text> shape;
        /** @brief Color or tint. */
        ecs::Rgba color;
    };

    /** @brief Elements, in the order they are drawn. */
    std::vector<Item> _items;
};

}  // namespace rlss::client
