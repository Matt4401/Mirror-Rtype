/**
 * @file UiButtonSystem.hpp
 * @brief System updating UiButton interaction states and dispatching click events.
 */

#pragma once

#include <span>

#include "core/services/IWindowService.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

/**
 * @brief Updates `UiButton` states each frame and dispatches `UiButtonClickedEvent`.
 *
 * Must run in the `update` phase, before `UiRenderSystem::collect`, so that
 * the render pass reads the current-frame state.
 *
 * Hit-testing uses the rectangles of a `computeLayout` pass, the same placement the
 * render system draws with, so buttons nested in a panel are hit where they are drawn.
 */
class UiButtonSystem {
  public:
    /** @brief Default constructor. */
    UiButtonSystem() = default;

    /**
     * @brief Processes every laid-out element that has a `UiButton`.
     *
     * For each button:
     * - Hit-tests the mouse against its layout rectangle.
     * - Transitions the state machine (Idle → Hovered → Pressed → Released → Hovered/Idle).
     * - Dispatches `UiButtonClickedEvent` on the Released → Hovered/Idle transition.
     *
     * @param registry Registry to read and mutate.
     * @param layout   Element rectangles from `computeLayout` for this frame.
     * @param window   Window service providing mouse position and button state.
     * @param bus      Event bus used to publish click events.
     */
    static void update(ecs::Registry& registry, std::span<const UiLayoutNode> layout,
                       const engine::IWindowService& window, ecs::EventBus& bus);
};

}  // namespace ui
