#pragma once

namespace ui {

/**
 * @brief Screen-space position for a HUD entity.
 *
 * The anchor is expressed as a normalized screen fraction (`anchorX`, `anchorY` in [0, 1])
 * plus a pixel offset. `{0, 0}` is the top-left corner, `{1, 1}` is the bottom-right.
 * Entities with this component are drawn in screen space, unaffected by the world camera.
 */
struct UiAnchor {
    /** @brief Normalized horizontal screen position in [0, 1]. */
    float anchorX = 0.0F;

    /** @brief Normalized vertical screen position in [0, 1]. */
    float anchorY = 0.0F;

    /** @brief Pixel offset applied after the anchor, on the X axis. */
    float offsetX = 0.0F;

    /** @brief Pixel offset applied after the anchor, on the Y axis. */
    float offsetY = 0.0F;
};

}  // namespace ui
