#pragma once

#include <cstdint>

namespace ecs {

using TextureId = std::uint32_t;
inline constexpr TextureId NO_TEXTURE = 0;

/**
 * @brief 8-bit RGBA color, independent of any graphics library.
 */
struct Rgba {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
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

    /** @brief Color multiplied with the texture; white leaves it unchanged. */
    Rgba tint;

    /** @brief Draw layer; entities on higher layers are drawn in front. */
    int layer = 0;
};

}  // namespace ecs
