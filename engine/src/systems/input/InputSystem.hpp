#pragma once

#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "input/ActionMapping.hpp"

namespace systems {

/**
 * @brief System that samples hardware inputs, updates the InputState resource, and publishes input action events.
 *
 * Polling is driven by dynamic action bindings (discrete buttons and continuous 1D axes) configured via ActionMapping.
 */
class InputSystem {
  public:
    /**
     * @brief Constructs an input system with configurable player and gamepad indices.
     * @param mapping Action mapping configuration.
     * @param playerIndex Logical index of the controlled player entity.
     * @param gamepadIndex Hardware index of the polled gamepad device.
     */
    explicit InputSystem(input::ActionMapping mapping = input::ActionMapping{}, int playerIndex = 0,
                         int gamepadIndex = 0);

    /**
     * @brief Polls inputs, updates the registry InputState resource, and publishes transition events.
     *
     * @param registry ECS registry containing entities and resources.
     * @param eventBus Optional event bus to publish InputActionEvent on state transitions.
     */
    void update(ecs::Registry& registry, ecs::EventBus* eventBus = nullptr);

    /** @brief Gets mutable reference to the internal action mapping configuration. */
    [[nodiscard]] input::ActionMapping& mapping() noexcept { return _mapping; }

    /** @brief Gets const reference to the internal action mapping configuration. */
    [[nodiscard]] const input::ActionMapping& mapping() const noexcept { return _mapping; }

    /** @brief Gets the logical player index handled by this system. */
    [[nodiscard]] int playerIndex() const noexcept { return _playerIndex; }

    /** @brief Sets the logical player index handled by this system. */
    void setPlayerIndex(int playerIndex) noexcept { _playerIndex = playerIndex; }

    /** @brief Gets the hardware gamepad device index polled by this system. */
    [[nodiscard]] int gamepadIndex() const noexcept { return _gamepadIndex; }

    /** @brief Sets the hardware gamepad device index polled by this system. */
    void setGamepadIndex(int gamepadIndex) noexcept { _gamepadIndex = gamepadIndex; }

  private:
    /** @brief Active input action mapping configuration. */
    input::ActionMapping _mapping;

    /** @brief Logical player index assigned to this input system instance. */
    int _playerIndex = 0;

    /** @brief Hardware gamepad device index polled by this input system instance. */
    int _gamepadIndex = 0;
};

}  // namespace systems
