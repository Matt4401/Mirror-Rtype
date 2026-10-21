#include "systems/input/InputSystem.hpp"

#include <utility>

#include "ecs/Entity.hpp"
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
    registry.runSystem<input::InputState>([this, eventBus](ecs::Entity /*entity*/, input::InputState& state) {
        if (state.playerIndex == _playerIndex) {
            updateState(state, eventBus);
        }
    });
}

void InputSystem::updateState(input::InputState& state, ecs::EventBus* eventBus) {
    state.resetFrameTransitions();

    for (const auto& [actionId, binding] : _mapping.bindings()) {
        if (binding.type == input::ActionType::Button) {
            const bool isDown = _mapping.isActionDown(actionId, _gamepadIndex);
            auto& actionData = state.actions[actionId];
            const bool wasDown = actionData.held;

            actionData.held = isDown;

            if (!wasDown && isDown) {
                actionData.justPressed = true;
                if (eventBus != nullptr) {
                    eventBus->publish(events::InputActionEvent{
                        .action = actionId,
                        .state = input::ActionState::Pressed,
                        .value = 1.0F,
                        .playerIndex = _playerIndex,
                    });
                }
            } else if (wasDown && !isDown) {
                actionData.justReleased = true;
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
            state.actions[actionId].axis = axisValue;
        }
    }
}

}  // namespace systems
