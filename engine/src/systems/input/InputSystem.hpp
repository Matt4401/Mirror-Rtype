#pragma once

#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "input/ActionMapping.hpp"

namespace systems {

/**
 * @brief System that samples hardware inputs, updates the InputState resource, and propagates intent to InputReceiver
 * entities.
 */
class InputSystem {
  public:
    /**
     * @brief Constructs an input system with optional custom action mappings.
     * @param mapping Action mapping configuration.
     */
    explicit InputSystem(input::ActionMapping mapping = input::ActionMapping{});

    /**
     * @brief Polls inputs, updates the registry resource and entities with InputReceiver.
     *
     * @param registry ECS registry containing entities and resources.
     * @param eventBus Optional event bus to publish InputActionEvent on state transitions.
     */
    void update(ecs::Registry& registry, ecs::EventBus* eventBus = nullptr);

    /** @brief Gets mutable reference to the internal action mapping configuration. */
    [[nodiscard]] input::ActionMapping& mapping() noexcept { return _mapping; }

    /** @brief Gets const reference to the internal action mapping configuration. */
    [[nodiscard]] const input::ActionMapping& mapping() const noexcept { return _mapping; }

  private:
    /** @brief Active input action mapping configuration. */
    input::ActionMapping _mapping;
};

}  // namespace systems
