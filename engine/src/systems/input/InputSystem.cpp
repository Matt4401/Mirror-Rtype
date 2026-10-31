#include "systems/input/InputSystem.hpp"

#include <utility>

#include "core/services/IWindowService.hpp"
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

void InputSystem::update(const engine::IWindowService& window, ecs::Registry& registry, ecs::EventBus* eventBus) {
    registry.runSystem<input::InputState>([this, &window, eventBus](ecs::Entity /*entity*/, input::InputState& state) {
        if (state.playerIndex == _playerIndex) {
            updateState(window, state, eventBus);
        }
    });
}

void InputSystem::updateState(const engine::IWindowService& window, input::InputState& state, ecs::EventBus* eventBus) {
    state.resetFrameTransitions();

    for (const auto& [actionId, binding] : _mapping.bindings()) {
        if (binding.type == input::ActionType::Button) {
            const bool isDown = _mapping.isActionDown(window, actionId, _gamepadIndex);
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
            const float axisValue = _mapping.getAxis(window, actionId, _gamepadIndex);
            state.actions[actionId].axis = axisValue;
        }
    }
}

}  // namespace systems
