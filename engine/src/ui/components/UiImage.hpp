/**
 * @file UiImage.hpp
 * @brief HUD component drawing a texture stretched over a rectangle.
 */

#pragma once

#include "components/Renderable.hpp"

namespace ui {

/** @brief A texture drawn in screen space, stretched to `width` x `height`, tinted. */
struct UiImage {
    /** @brief Texture to draw. */
    ecs::TextureId texture = ecs::NO_TEXTURE;
    /** @brief Width on screen in pixels. */
    float width = 0.0F;
    /** @brief Height on screen in pixels. */
    float height = 0.0F;
    /** @brief Tint (and opacity). */
    ecs::Rgba tint = {.r = 255, .g = 255, .b = 255, .a = 255};
    /** @brief Part of the texture drawn; empty for the whole texture. */
    ecs::TextureRect source{};
};

}  // namespace ui
