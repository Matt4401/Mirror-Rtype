#pragma once
#include <raylib.h>

namespace render {

/**
 * @brief Camera used by the rendering system to view the world.
 *
 * Wraps raylib's `Camera2D`: `target` is the world point the camera looks at,
 * `offset` is where that point appears on screen, and `zoom` scales the view.
 * Lives in the rendering module because it depends on raylib.
 */
struct CameraComponent {
    /** @brief Underlying raylib camera, passed to `BeginMode2D` when drawing. */
    Camera2D camera{.offset = {.x = 0.0F, .y = 0.0F}, .target = {.x = 0.0F, .y = 0.0F}, .rotation = 0.0F, .zoom = 1.0F};

    /** @brief Only the active camera is used to draw the world. */
    bool active = true;
};

}  // namespace render
