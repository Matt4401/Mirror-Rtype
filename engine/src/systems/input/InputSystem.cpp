#include "systems/input/InputSystem.hpp"

#include <utility>

#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/InputActionEvent.hpp"
#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"
#include "input/InputState.hpp"

namespace systems {

InputSystem::InputSystem(input::ActionMapping mapping, int playerIndex, int gamepadIndex)
    : _mapping(std::move(mapping)), _playerIndex(playerIndex), _gamepadIndex(gamepadIndex) {}

void InputSystem::update(ecs::Registry& registry, ecs::EventBus* eventBus) {
    if (!registry.hasResource<input::InputState>()) {
        registry.emplaceResource<input::InputState>();
    }
    auto& state = registry.getResource<input::InputState>();
    state.resetFrameTransitions();

    for (const auto& [actionId, binding] : _mapping.bindings()) {
        if (binding.type == input::ActionType::Button) {
            const bool isDown = _mapping.isActionDown(actionId, _gamepadIndex);
            const bool wasDown = state.isDown(actionId);

            state.setDown(actionId, isDown);

            if (!wasDown && isDown) {
                state.setJustPressed(actionId, true);
                if (eventBus != nullptr) {
                    eventBus->publish(events::InputActionEvent{
                        .action = actionId,
                        .state = input::ActionState::Pressed,
                        .value = 1.0F,
                        .playerIndex = _playerIndex,
                    });
                }
            } else if (wasDown && !isDown) {
                state.setJustReleased(actionId, true);
                if (eventBus != nullptr) {
                    eventBus->publish(events::InputActionEvent{
                        .action = actionId,
                        .state = input::ActionState::Released,
                        .value = 0.0F,
                        .playerIndex = _playerIndex,
                    });
                }
            }
        } else if (binding.type == input::ActionType::Axis) {
            const float axisValue = _mapping.getAxis(actionId, _gamepadIndex);
            state.setAxis(actionId, axisValue);
        }
    }
}

}  // namespace systems
