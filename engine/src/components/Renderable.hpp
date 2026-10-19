#pragma once

#include <cstdint>

namespace ecs {

using TextureId = std::uint32_t;
inline constexpr TextureId NO_TEXTURE = 0;

/**
 * @brief 8-bit RGBA color, independent of any graphics library.
 */
struct Rgba {
    /** @brief Red color component [0, 255]. */
    std::uint8_t r = 255;

    /** @brief Green color component [0, 255]. */
    std::uint8_t g = 255;

    /** @brief Blue color component [0, 255]. */
    std::uint8_t b = 255;

    /** @brief Alpha opacity component [0, 255]. */
    std::uint8_t a = 255;
};

/**
 * @brief Rectangular pixel region inside a texture.
 *
 * A zero width and height select the complete texture.
 */
struct TextureRect {
    /** @brief Horizontal pixel coordinate of top-left corner in texture space. */
    int x = 0;

    /** @brief Vertical pixel coordinate of top-left corner in texture space. */
    int y = 0;

    /** @brief Width in pixels of the source sub-rectangle. */
    int width = 0;

    /** @brief Height in pixels of the source sub-rectangle. */
    int height = 0;
};

/**
 * @brief Normalized render pivot within the drawable rectangle.
 *
 * `{0.0F, 0.0F}` is the top-left corner and `{0.5F, 0.5F}` is the center.
 */
struct RenderOrigin {
    /** @brief Horizontal normalized pivot position in [0.0F, 1.0F]. */
    float x = 0.5F;

    /** @brief Vertical normalized pivot position in [0.0F, 1.0F]. */
    float y = 0.5F;
};

/**
 * @brief Marks an entity as drawable by the rendering system.
 *
 * Plain data with no graphics-library types, so the headless server can carry it
 * without linking any rendering code. The rendering module maps `textureId` to an actual texture.
 */
struct Renderable {
    /** @brief Texture to draw, or `NO_TEXTURE`. */
    TextureId textureId = NO_TEXTURE;

    /** @brief Texture region to draw; a zero-sized region draws the complete texture. */
    TextureRect sourceRect;

    /** @brief Normalized pivot used for alignment and rotation during rendering. */
    RenderOrigin origin;

    /** @brief Color multiplied with the texture; white leaves it unchanged. */
    Rgba tint;

    /** @brief Draw layer; entities on higher layers are drawn in front. */
    int layer = 0;
};

}  // namespace ecs
