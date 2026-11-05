/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** UiButtonSystem implementation.
*/

#include "ui/system/UiButtonSystem.hpp"

#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/UiEvents.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"

namespace ui {

void UiButtonSystem::update(ecs::Registry& registry, const engine::IWindowService& window, ecs::EventBus& bus) {
    const auto screenW = static_cast<float>(window.width());
    const auto screenH = static_cast<float>(window.height());
    const engine::MousePosition mouse = window.mousePosition();
    const bool mouseDown = window.isMouseButtonDown(engine::MouseButton::Left);

    registry.runSystem<UiAnchor, UiButton>([&](ecs::Entity entity, const UiAnchor& anchor, UiButton& btn) {
        const float x = (anchor.anchorX * screenW) + anchor.offsetX - (anchor.pivotX * btn.width);
        const float y = (anchor.anchorY * screenH) + anchor.offsetY - (anchor.pivotY * btn.height);

        const bool hovered = mouse.x >= x && mouse.x <= x + btn.width && mouse.y >= y && mouse.y <= y + btn.height;

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
    });
}

}  // namespace ui
