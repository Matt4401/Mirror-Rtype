/**
 * @file IScreenDrawService.hpp
 * @brief Abstract service drawing 2D primitives in screen space, on top of the world.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "components/Renderable.hpp"

namespace engine {

/** @brief Identifier of a font loaded by `IScreenDrawService::loadFont`. */
using FontId = std::uint32_t;

/** @brief Sentinel selecting the backend's default font. */
inline constexpr FontId NO_FONT = 0;

/** @brief Axis-aligned rectangle in screen pixels, origin at the top-left corner of the window. */
struct ScreenRect {
    /** @brief Left edge in pixels. */
    float x = 0.0F;

    /** @brief Top edge in pixels. */
    float y = 0.0F;

    /** @brief Width in pixels. */
    float width = 0.0F;

    /** @brief Height in pixels. */
    float height = 0.0F;
};

/** @brief Size of a piece of rendered text in pixels. */
struct TextSize {
    /** @brief Width in pixels. */
    float width = 0.0F;

    /** @brief Height in pixels. */
    float height = 0.0F;
};

/**
 * @brief Screen-space 2D drawing, exposed through the Mediator by the render backend.
 *
 * Lets overlay systems (HUD, menus, debug views) draw without depending on the graphics library. The draw calls are
 * **queued**: the backend draws them on top of the world, in submission order, during its render pass, then clears
 * the queue. Submit them before the render phase (`update` or `lateUpdate`) so they show up in the same frame.
 */
class IScreenDrawService {
  public:
    /** @brief Default constructor. */
    IScreenDrawService() = default;

    /** @brief Virtual destructor for polymorphic cleanup. */
    virtual ~IScreenDrawService() = default;

    /** @brief Default copy constructor. */
    IScreenDrawService(const IScreenDrawService&) = default;

    /** @brief Default copy assignment operator. */
    IScreenDrawService& operator=(const IScreenDrawService&) = default;

    /** @brief Default move constructor. */
    IScreenDrawService(IScreenDrawService&&) noexcept = default;

    /** @brief Default move assignment operator. */
    IScreenDrawService& operator=(IScreenDrawService&&) noexcept = default;

    /**
     * @brief Loads a font file.
     * @param path Path to the font file.
     * @return Identifier of the font, or `NO_FONT` if it could not be loaded.
     */
    virtual FontId loadFont(const std::string& path) = 0;

    /**
     * @brief Measures a piece of text as `drawText` would draw it.
     * @param text Text to measure.
     * @param fontSize Font size in pixels.
     * @param spacing Extra space between characters in pixels.
     * @param font Font to use; `NO_FONT` or an unknown id selects the default font.
     * @return Size of the text in pixels.
     */
    [[nodiscard]] virtual TextSize measureText(std::string_view text, float fontSize, float spacing,
                                               FontId font) const = 0;

    /**
     * @brief Queues a filled rectangle.
     * @param rect Rectangle in screen pixels.
     * @param color Fill color.
     */
    virtual void fillRect(const ScreenRect& rect, const ecs::Rgba& color) = 0;

    /**
     * @brief Queues a rectangle outline, drawn inside the rectangle.
     * @param rect Rectangle in screen pixels.
     * @param thickness Line thickness in pixels.
     * @param color Line color.
     */
    virtual void strokeRect(const ScreenRect& rect, float thickness, const ecs::Rgba& color) = 0;

    /**
     * @brief Queues a piece of text.
     * @param text Text to draw; copied, so it may be a temporary.
     * @param x Left edge in pixels.
     * @param y Top edge in pixels.
     * @param fontSize Font size in pixels.
     * @param spacing Extra space between characters in pixels.
     * @param color Text color.
     * @param font Font to use; `NO_FONT` or an unknown id selects the default font.
     */
    virtual void drawText(std::string_view text, float x, float y, float fontSize, float spacing,
                          const ecs::Rgba& color, FontId font) = 0;

    /**
     * @brief Queues a textured quad.
     * @param texture Texture loaded by the render backend; an unknown id draws nothing.
     * @param source Region of the texture; a zero-sized region selects the whole texture.
     * @param dest Rectangle in screen pixels the region is stretched over (before rotation).
     * @param rotation Rotation around the center of `dest`, in radians, clockwise on screen.
     * @param tint Color multiplied with the texture; white leaves it unchanged.
     */
    virtual void drawTexture(ecs::TextureId texture, const ecs::TextureRect& source, const ScreenRect& dest,
                             float rotation, const ecs::Rgba& tint) = 0;
};

}  // namespace engine
