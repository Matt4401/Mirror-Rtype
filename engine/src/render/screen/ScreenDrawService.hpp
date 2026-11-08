/**
 * @file ScreenDrawService.hpp
 * @brief Raylib implementation of IScreenDrawService.
 */

#pragma once

#include <raylib.h>

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "render/font/FontStore.hpp"
#include "render/texture/TextureStore.hpp"

namespace render {

/**
 * @brief Queues screen-space draw calls and draws them with raylib.
 *
 * Owned by `RenderPlugin`, which calls `flush` after the world pass and before ending the frame, so overlays are
 * drawn on top of the world in the frame they were submitted. Owns the fonts it loads; must be destroyed before the
 * window.
 */
class ScreenDrawService final : public engine::IScreenDrawService {
  public:
    /**
     * @brief Creates an empty service.
     * @param textures Store `drawTexture` reads from; it must outlive the service. Null draws no texture.
     */
    explicit ScreenDrawService(const TextureStore* textures = nullptr) noexcept : _textures(textures) {}

    /**
     * @brief Loads a font file.
     * @param path Path to the font file.
     * @return Identifier of the font, or `engine::NO_FONT` on failure.
     */
    engine::FontId loadFont(const std::string& path) override;

    /**
     * @brief Releases a font loaded by `loadFont`; unknown identifiers are ignored.
     * @param font Identifier of the font.
     */
    void unloadFont(engine::FontId font);

    /**
     * @brief Measures text with the given font.
     * @param text Text to measure.
     * @param fontSize Font size in pixels.
     * @param spacing Extra space between characters in pixels.
     * @param font Font to use; `engine::NO_FONT` or an unknown id selects the default font.
     * @return Size of the text in pixels.
     */
    [[nodiscard]] engine::TextSize measureText(std::string_view text, float fontSize, float spacing,
                                               engine::FontId font) const override;

    /**
     * @brief Queues a filled rectangle.
     * @param rect Rectangle in screen pixels.
     * @param color Fill color.
     */
    void fillRect(const engine::ScreenRect& rect, const ecs::Rgba& color) override;

    /**
     * @brief Queues a rectangle outline.
     * @param rect Rectangle in screen pixels.
     * @param thickness Line thickness in pixels.
     * @param color Line color.
     */
    void strokeRect(const engine::ScreenRect& rect, float thickness, const ecs::Rgba& color) override;

    /**
     * @brief Queues a piece of text.
     * @param text Text to draw (copied).
     * @param x Left edge in pixels.
     * @param y Top edge in pixels.
     * @param fontSize Font size in pixels.
     * @param spacing Extra space between characters in pixels.
     * @param color Text color.
     * @param font Font to use; `engine::NO_FONT` or an unknown id selects the default font.
     */
    void drawText(std::string_view text, float x, float y, float fontSize, float spacing, const ecs::Rgba& color,
                  engine::FontId font) override;

    /**
     * @brief Queues a textured quad.
     * @param texture Texture of the store given at construction; an unknown id draws nothing.
     * @param source Region of the texture; a zero-sized region selects the whole texture.
     * @param dest Rectangle in screen pixels.
     * @param rotation Rotation around the center of `dest`, in radians.
     * @param tint Color multiplied with the texture.
     */
    void drawTexture(ecs::TextureId texture, const ecs::TextureRect& source, const engine::ScreenRect& dest,
                     float rotation, const ecs::Rgba& tint) override;

    /**
     * @brief Draws every queued call in submission order, then empties the queue.
     *
     * Must be called between `Window::beginFrame` and `Window::endFrame`.
     */
    void flush();

  private:
    /** @brief Queued filled rectangle. */
    struct FillCommand {
        /** @brief Rectangle in screen pixels. */
        engine::ScreenRect rect;
        /** @brief Fill color. */
        ecs::Rgba color;
    };

    /** @brief Queued rectangle outline. */
    struct StrokeCommand {
        /** @brief Rectangle in screen pixels. */
        engine::ScreenRect rect;
        /** @brief Line thickness in pixels. */
        float thickness = 0.0F;
        /** @brief Line color. */
        ecs::Rgba color;
    };

    /** @brief Queued text. */
    struct TextCommand {
        /** @brief Owned copy of the text. */
        std::string text;
        /** @brief Left edge in pixels. */
        float x = 0.0F;
        /** @brief Top edge in pixels. */
        float y = 0.0F;
        /** @brief Font size in pixels. */
        float fontSize = 0.0F;
        /** @brief Extra space between characters in pixels. */
        float spacing = 0.0F;
        /** @brief Text color. */
        ecs::Rgba color;
        /** @brief Font identifier. */
        engine::FontId font = engine::NO_FONT;
    };

    /** @brief Queued textured quad. */
    struct TextureCommand {
        /** @brief Texture identifier. */
        ecs::TextureId texture = ecs::NO_TEXTURE;
        /** @brief Region of the texture. */
        ecs::TextureRect source;
        /** @brief Rectangle in screen pixels. */
        engine::ScreenRect dest;
        /** @brief Rotation around the center of `dest`, in radians. */
        float rotation = 0.0F;
        /** @brief Tint. */
        ecs::Rgba tint;
    };

    /** @brief Any queued draw call. */
    using Command = std::variant<FillCommand, StrokeCommand, TextCommand, TextureCommand>;

    /**
     * @brief Draws one queued textured quad.
     * @param command The quad.
     */
    void drawQueued(const TextureCommand& command) const;

    /**
     * @brief Resolves a font identifier, falling back to the default font.
     * @param font Font identifier.
     * @return Font to draw or measure with.
     */
    [[nodiscard]] Font resolveFont(engine::FontId font) const;

    /** @brief Fonts loaded through `loadFont`. */
    FontStore _fonts;
    /** @brief Textures `drawTexture` reads from (not owned), or null. */
    const TextureStore* _textures = nullptr;
    /** @brief Draw calls of the current frame; capacity is kept between frames. */
    std::vector<Command> _commands;
};

}  // namespace render
