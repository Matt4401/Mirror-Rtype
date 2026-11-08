/**
 * @file UiInteractionSystem.hpp
 * @brief Focus navigation, text inputs, sliders, toggles and list scrolling.
 */

#pragma once

#include <span>
#include <vector>

#include "core/services/IWindowService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

/**
 * @brief Runs every UI interaction that is not a plain mouse click on a button (see `UiButtonSystem` for those).
 *
 * - **Focus**: a click on a `UiFocusable` widget focuses it, a click elsewhere clears the focus. Up/down (and
 *   left/right, except on sliders) move it through the visible focusable widgets. Inside a `UiScrollList`, they move
 *   from row to row and scroll the list.
 * - **Confirm** on the focused widget clicks a button, flips a toggle, or submits a text input.
 * - **Back** publishes `UiBackEvent`.
 * - **Text input**: typed characters, Backspace with key repeat, blinking caret.
 * - **Slider**: dragged with the mouse, or moved by its step with left/right.
 * - **Scroll list**: the mouse wheel over it scrolls it.
 *
 * Bindings come from the `UiNavigation` resource and the focus is the `UiFocus` resource (both emplaced if missing).
 * The system only reads the window service and keeps its scratch buffers, so a frame without input does not allocate.
 */
class UiInteractionSystem {
  public:
    /** @brief Hold time before Backspace starts repeating, in seconds. */
    static constexpr float BACKSPACE_DELAY = 0.45F;
    /** @brief Time between two repeated Backspaces, in seconds. */
    static constexpr float BACKSPACE_REPEAT = 0.05F;
    /** @brief Half period of the caret blink, in seconds. */
    static constexpr float CARET_BLINK = 0.5F;

    /**
     * @brief Runs the interactions of one frame.
     * @param registry Registry holding the UI.
     * @param layout Layout of this frame (only these widgets are visible).
     * @param window Window service (keyboard, gamepad, mouse, typed characters).
     * @param bus Bus receiving the UI events.
     * @param dt Frame time in seconds (key repeat, caret blink).
     */
    void update(ecs::Registry& registry, std::span<const UiLayoutNode> layout, engine::IWindowService& window,
                ecs::EventBus& bus, float dt);

  private:
    /** @brief State of the navigation commands for one frame. */
    struct Commands {
        /** @brief Up held. */
        bool up = false;
        /** @brief Down held. */
        bool down = false;
        /** @brief Left held. */
        bool left = false;
        /** @brief Right held. */
        bool right = false;
        /** @brief Confirm held, by any binding. */
        bool confirm = false;
        /** @brief Confirm held by a binding that is not a printable key (Space types in text inputs). */
        bool confirmNotTyping = false;
        /** @brief Back held. */
        bool back = false;
    };

    /**
     * @brief Reads which commands are held now.
     * @param window Window service.
     * @param navigation Bindings.
     * @return Held commands.
     */
    [[nodiscard]] static Commands readCommands(const engine::IWindowService& window, const UiNavigation& navigation);

    /**
     * @brief Handles clicks, drags and the wheel.
     * @param registry Registry holding the UI.
     * @param layout Layout of this frame.
     * @param window Window service.
     * @param bus Event bus.
     * @param focus Focus resource.
     */
    void handleMouse(ecs::Registry& registry, std::span<const UiLayoutNode> layout,
                     const engine::IWindowService& window, ecs::EventBus& bus, UiFocus& focus);

    /**
     * @brief Focuses the widget under a click (or clears the focus), flips toggles, starts slider drags.
     * @param registry Registry holding the UI.
     * @param layout Layout of this frame.
     * @param mouse Cursor position.
     * @param bus Event bus.
     * @param focus Focus resource.
     */
    void handleClick(ecs::Registry& registry, std::span<const UiLayoutNode> layout, engine::MousePosition mouse,
                     ecs::EventBus& bus, UiFocus& focus);

    /**
     * @brief Moves the dragged slider under the cursor.
     * @param registry Registry holding the UI.
     * @param layout Layout of this frame.
     * @param mouse Cursor position.
     * @param bus Event bus.
     */
    void drag(ecs::Registry& registry, std::span<const UiLayoutNode> layout, engine::MousePosition mouse,
              ecs::EventBus& bus);

    /**
     * @brief Scrolls the list under the cursor by one row.
     * @param registry Registry holding the UI.
     * @param layout Layout of this frame.
     * @param mouse Cursor position.
     * @param wheel Wheel movement; positive scrolls up.
     */
    static void scroll(ecs::Registry& registry, std::span<const UiLayoutNode> layout, engine::MousePosition mouse,
                       float wheel);

    /**
     * @brief Moves the focus by one step.
     * @param registry Registry holding the UI.
     * @param layout Layout of this frame.
     * @param focus Focus resource.
     * @param forward `true` for next, `false` for previous.
     */
    void moveFocus(ecs::Registry& registry, std::span<const UiLayoutNode> layout, UiFocus& focus, bool forward);

    /**
     * @brief Moves the focus to the next or previous row of the scroll list holding the focused widget.
     * @param registry Registry holding the UI.
     * @param focus Focus resource.
     * @param forward `true` for the next row.
     * @return `true` if the focus moved inside a list.
     */
    static bool moveInList(ecs::Registry& registry, UiFocus& focus, bool forward);

    /**
     * @brief Activates the focused widget (confirm).
     * @param registry Registry holding the UI.
     * @param bus Event bus.
     * @param focused Focused widget.
     */
    static void activate(ecs::Registry& registry, ecs::EventBus& bus, ecs::Entity focused);

    /**
     * @brief Edits the focused text input from typed characters and Backspace.
     * @param registry Registry holding the UI.
     * @param window Window service.
     * @param focused Focused text input.
     * @param focus Focus resource (caret).
     * @param dt Frame time.
     */
    void editText(ecs::Registry& registry, engine::IWindowService& window, ecs::Entity focused, UiFocus& focus,
                  float dt);

    /** @brief Commands held during the previous frame, for edge detection. */
    Commands _previous;
    /** @brief Focusable widgets of the frame, in navigation order; capacity kept between frames. */
    std::vector<ecs::Entity> _focusables;
    /** @brief Slider being dragged with the mouse. */
    ecs::Entity _dragged = ecs::NULL_ENTITY;
    /** @brief Backspace held during the previous frame. */
    bool _backspaceHeld = false;
    /** @brief Time until the next repeated Backspace. */
    float _backspaceTimer = 0.0F;
    /** @brief Time since the caret last changed phase. */
    float _caretTimer = 0.0F;
    /** @brief Widget focused during the previous frame, to restart the caret on a focus change. */
    ecs::Entity _lastFocused = ecs::NULL_ENTITY;
};

}  // namespace ui
