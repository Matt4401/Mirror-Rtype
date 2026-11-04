/**
 * @file UiButtonSystem.hpp
 * @brief System updating UiButton interaction states and dispatching click events.
 */

#pragma once

#include "core/services/IWindowService.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"

namespace ui {

/**
 * @brief Updates `UiButton` states each frame and dispatches `UiButtonClickedEvent`.
 *
 * Must run in the `update` phase, before `UiRenderSystem::collect`, so that
 * the render pass reads the current-frame state.
 *
 * Hit-testing uses the same anchor formula as `UiRenderSystem`, querying the
 * live screen dimensions from raylib at call time.
 */
class UiButtonSystem {
  public:
    /** @brief Default constructor. */
    UiButtonSystem() = default;

    /**
     * @brief Processes all `UiAnchor` + `UiButton` entities.
     *
     * For each button:
     * - Resolves its screen rectangle using the anchor and pivot.
     * - Transitions the state machine (Idle → Hovered → Pressed → Released → Hovered/Idle).
     * - Dispatches `UiButtonClickedEvent` on the Released → Hovered/Idle transition.
     *
     * @param registry Registry to read and mutate.
     * @param window   Window service providing mouse position and button state.
     * @param bus      Event bus used to publish click events.
     */
    void update(ecs::Registry& registry, const engine::IWindowService& window, ecs::EventBus& bus);
};

}  // namespace ui
