#include "systems/input/InputSystem.hpp"

#include <cmath>
#include <cstddef>

#include "components/InputReceiver.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/InputActionEvent.hpp"
#include "input/ActionMapping.hpp"
#include "input/InputAction.hpp"
#include "input/InputState.hpp"

namespace systems {

InputSystem::InputSystem(input::ActionMapping mapping) : _mapping(mapping) {}

void InputSystem::update(ecs::Registry& registry, ecs::EventBus* eventBus) {
    if (!registry.hasResource<input::InputState>()) {
        registry.emplaceResource<input::InputState>();
    }
    auto& state = registry.getResource<input::InputState>();
    state.resetFrameTransitions();

    for (std::size_t i = 0; i < input::ACTION_COUNT; ++i) {
        const auto action = static_cast<input::Action>(i);
        const bool isDown = _mapping.isActionDown(action, 0);
        const bool wasDown = state.isDown(action);

        state.setDown(action, isDown);

        if (!wasDown && isDown) {
            state.setJustPressed(action, true);
            if (eventBus != nullptr) {
                eventBus->publish(events::InputActionEvent{
                    .action = action,
                    .state = input::ActionState::Pressed,
                    .value = 1.0F,
                    .playerIndex = 0,
                });
            }
        } else if (wasDown && !isDown) {
            state.setJustReleased(action, true);
            if (eventBus != nullptr) {
                eventBus->publish(events::InputActionEvent{
                    .action = action,
                    .state = input::ActionState::Released,
                    .value = 0.0F,
                    .playerIndex = 0,
                });
            }
        }
    }

    float moveX = 0.0F;
    float moveY = 0.0F;
    if (state.isDown(input::Action::MoveRight)) {
        moveX += 1.0F;
    }
    if (state.isDown(input::Action::MoveLeft)) {
        moveX -= 1.0F;
    }
    if (state.isDown(input::Action::MoveDown)) {
        moveY += 1.0F;
    }
    if (state.isDown(input::Action::MoveUp)) {
        moveY -= 1.0F;
    }

    const float lenSq = (moveX * moveX) + (moveY * moveY);
    if (lenSq > 1.0F) {
        const float invLen = 1.0F / std::sqrt(lenSq);
        moveX *= invLen;
        moveY *= invLen;
    }
    state.setAxes(moveX, moveY);

    registry.runSystem<ecs::InputReceiver>([&state](ecs::Entity /*entity*/, ecs::InputReceiver& receiver) {
        if (!receiver.enabled) {
            return;
        }
        receiver.moveX = state.axisX();
        receiver.moveY = state.axisY();
        receiver.shooting = state.isDown(input::Action::Shoot);
        receiver.charging = state.isDown(input::Action::ChargeWeapon);
    });
}

}  // namespace systems
