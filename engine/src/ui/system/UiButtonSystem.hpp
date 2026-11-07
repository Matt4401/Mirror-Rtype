/**
 * @file UiButtonSystem.hpp
 * @brief System updating UiButton interaction states and dispatching click events.
 */

#pragma once

#include <span>

#include "core/services/IWindowService.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

/** @brief State of the left mouse button relative to one button, for a single frame. */
struct UiPointerSample {
    /** @brief The cursor is over the button. */
    bool hovered = false;

    /** @brief The mouse button is held down. */
    bool down = false;

    /** @brief The mouse button went down this frame. */
    bool pressed = false;

    /** @brief The mouse button went up this frame. */
    bool released = false;
};

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
     * @brief Computes the next state of a button.
     *
     * - Idle / Hovered / Released: `Pressed` if the press starts over the button this frame, otherwise `Hovered` or
     *   `Idle` depending on the cursor. `Released` therefore lasts exactly one frame.
     * - Pressed: stays `Pressed` while held; on release, `Released` if the cursor is still over the button (a click),
     *   `Idle` otherwise (the click is cancelled).
     *
     * @param current State of the button on the previous frame.
     * @param pointer Mouse state for this frame.
     * @return New state of the button.
     */
    [[nodiscard]] static UiButtonState nextState(UiButtonState current, const UiPointerSample& pointer) noexcept;

    /**
     * @brief Processes every laid-out element that has a `UiButton`.
     *
     * For each button:
     * - Hit-tests the mouse against its layout rectangle.
     * - Applies `nextState`.
     * - Dispatches `UiButtonClickedEvent` when the button enters `Released`, i.e. only when the press is released
     *   over the button.
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
