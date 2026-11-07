/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** UiButtonSystem implementation.
*/

#include "ui/system/UiButtonSystem.hpp"

#include <span>

#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/UiEvents.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

UiButtonState UiButtonSystem::nextState(UiButtonState current, const UiPointerSample& pointer) noexcept {
    if (current == UiButtonState::Pressed) {
        // A release missed while the window was unfocused counts as a release, so the button never stays stuck.
        if (!pointer.released && pointer.down) {
            return UiButtonState::Pressed;
        }
        // Releasing outside the button cancels the click.
        return pointer.hovered ? UiButtonState::Released : UiButtonState::Idle;
    }
    // Idle, Hovered and Released (which lasts a single frame) behave the same. Only a press that starts over the
    // button arms it: dragging an already held button onto it does not.
    if (!pointer.hovered) {
        return UiButtonState::Idle;
    }
    return pointer.pressed ? UiButtonState::Pressed : UiButtonState::Hovered;
}

void UiButtonSystem::update(ecs::Registry& registry, std::span<const UiLayoutNode> layout,
                            const engine::IWindowService& window, ecs::EventBus& bus) {
    const engine::MousePosition mouse = window.mousePosition();
    UiPointerSample pointer{
        .down = window.isMouseButtonDown(engine::MouseButton::Left),
        .pressed = window.isMouseButtonPressed(engine::MouseButton::Left),
        .released = window.isMouseButtonReleased(engine::MouseButton::Left),
    };

    for (const UiLayoutNode& node : layout) {
        if (!registry.hasComponent<UiButton>(node.entity)) {
            continue;
        }
        UiButton& btn = registry.getComponents<UiButton>().get(node.entity);
        const UiScreenRect& rect = node.rect;
        pointer.hovered =
            mouse.x >= rect.x && mouse.x <= rect.x + rect.width && mouse.y >= rect.y && mouse.y <= rect.y + rect.height;

        btn.state = nextState(btn.state, pointer);
        if (btn.state == UiButtonState::Released) {
            bus.publish(UiButtonClickedEvent{.entity = node.entity, .action = btn.action});
        }
    }
}

}  // namespace ui
