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
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/UiEvents.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

void UiButtonSystem::update(ecs::Registry& registry, std::span<const UiLayoutNode> layout,
                            const engine::IWindowService& window, ecs::EventBus& bus) {
    const engine::MousePosition mouse = window.mousePosition();
    const bool mouseDown = window.isMouseButtonDown(engine::MouseButton::Left);

    for (const UiLayoutNode& node : layout) {
        if (!registry.hasComponent<UiButton>(node.entity)) {
            continue;
        }
        const ecs::Entity entity = node.entity;
        UiButton& btn = registry.getComponents<UiButton>().get(entity);
        const UiScreenRect& rect = node.rect;

        const bool hovered =
            mouse.x >= rect.x && mouse.x <= rect.x + rect.width && mouse.y >= rect.y && mouse.y <= rect.y + rect.height;

        switch (btn.state) {
            case UiButtonState::Idle:
                if (hovered) {
                    btn.state = UiButtonState::Hovered;
                }
                break;
            case UiButtonState::Hovered:
                if (!hovered) {
                    btn.state = UiButtonState::Idle;
                } else if (mouseDown) {
                    btn.state = UiButtonState::Pressed;
                }
                break;
            case UiButtonState::Pressed:
                if (!mouseDown) {
                    btn.state = hovered ? UiButtonState::Hovered : UiButtonState::Idle;
                    bus.publish(UiButtonClickedEvent{.entity = entity, .action = btn.action});
                }
                break;
            case UiButtonState::Released:
                btn.state = hovered ? UiButtonState::Hovered : UiButtonState::Idle;
                break;
        }
    }
}

}  // namespace ui
