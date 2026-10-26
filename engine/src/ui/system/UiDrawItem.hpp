#pragma once

#include <variant>

#include "ecs/Entity.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"

namespace ui {

/**
 * @brief One HUD entity captured for the current frame.
 *
 * Produced by `UiRenderSystem::collect`; the screen position is resolved from
 * the entity's `UiAnchor` at collection time.
 */
struct UiDrawItem {
    /** @brief Entity being drawn. */
    ecs::Entity entity = 0;

    /** @brief Resolved horizontal screen position in pixels. */
    float screenX = 0.0F;

    /** @brief Resolved vertical screen position in pixels. */
    float screenY = 0.0F;

    /** @brief The drawable component copied from the entity. */
    std::variant<UiRect, UiText, UiProgressBar> drawable;
};

}  // namespace ui
