/**
 * @file InputPlugin.hpp
 * @brief Input lifecycle plugin sampling hardware state and updating ECS InputState.
 */

#pragma once

#include <memory>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "input/ActionMapping.hpp"
#include "systems/input/InputSystem.hpp"

namespace input {

/**
 * @brief Plugin responsible for polling user inputs and publishing InputActionEvent transitions.
 */
class InputPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Constructs InputPlugin with mapping and hardware indices.
     * @param mapping Action mapping configuration.
     * @param playerIndex Controlled logical player index.
     * @param gamepadIndex Polled hardware gamepad index.
     */
    explicit InputPlugin(ActionMapping mapping = ActionMapping{}, int playerIndex = 0, int gamepadIndex = 0);

    /** @brief Destructor. */
    ~InputPlugin() override;

    /** @brief Deleted copy constructor. */
    InputPlugin(const InputPlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    InputPlugin& operator=(const InputPlugin&) = delete;

    /** @brief Deleted move constructor. */
    InputPlugin(InputPlugin&&) = delete;

    /** @brief Deleted move assignment operator. */
    InputPlugin& operator=(InputPlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "InputPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "InputPlugin"; }

    /**
     * @brief Declares plugin dependencies (requires CorePlugin and a window service).
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Registers IInputService in the engine Mediator.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Polls hardware inputs once per frame, before the fixed-timestep ticks consume InputState.
     * @param engine Reference to host Engine.
     */
    void preUpdate(engine::Engine& engine) override;

    /**
     * @brief Unregisters services from Mediator.
     * @param engine Reference to host Engine.
     */
    void shutdown(engine::Engine& engine) override;

    /** @brief Gets mutable reference to the underlying InputSystem. */
    [[nodiscard]] systems::InputSystem& inputSystem() noexcept { return _inputSystem; }

    /** @brief Gets const reference to the underlying InputSystem. */
    [[nodiscard]] const systems::InputSystem& inputSystem() const noexcept { return _inputSystem; }

    /** @brief Gets mutable reference to action mapping configuration. */
    [[nodiscard]] ActionMapping& mapping() noexcept { return _inputSystem.mapping(); }

  private:
    class InputServiceImpl;

    /** @brief Underlying input system monitoring hardware device states. */
    systems::InputSystem _inputSystem;
    /** @brief Shared pointer to abstract input service adapter registered in Mediator. */
    std::shared_ptr<InputServiceImpl> _inputService = nullptr;
};

}  // namespace input
