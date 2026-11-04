/**
 * @file InputPlugin.cpp
 * @brief Implementation of InputPlugin and its abstract service adapter.
 */

#include "input/InputPlugin.hpp"

#include <memory>
#include <utility>

#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/services/IInputService.hpp"
#include "core/services/IWindowService.hpp"
#include "systems/input/InputSystem.hpp"

namespace input {

/**
 * @brief Concrete adapter implementing IInputService via InputSystem.
 */
class InputPlugin::InputServiceImpl final : public engine::IInputService {
  public:
    /**
     * @brief Constructs InputServiceImpl wrapping an InputSystem reference.
     * @param system Reference to the active InputSystem.
     */
    explicit InputServiceImpl(systems::InputSystem& system) : _system(&system) {}

    [[nodiscard]] ActionMapping& mapping() noexcept override { return _system->mapping(); }

    [[nodiscard]] const ActionMapping& mapping() const noexcept override { return _system->mapping(); }

    [[nodiscard]] int playerIndex() const noexcept override { return _system->playerIndex(); }

    [[nodiscard]] int gamepadIndex() const noexcept override { return _system->gamepadIndex(); }

  private:
    /** @brief Pointer to wrapped InputSystem. */
    systems::InputSystem* _system;
};

InputPlugin::InputPlugin(ActionMapping mapping, int playerIndex, int gamepadIndex)
    : _inputSystem(std::move(mapping), playerIndex, gamepadIndex) {}

InputPlugin::~InputPlugin() = default;

void InputPlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
    builder.requireService<engine::IWindowService>();
    builder.provides<engine::IInputService>();
}

void InputPlugin::init(engine::Engine& engine) {
    _inputService = std::make_shared<InputServiceImpl>(_inputSystem);
    if (const auto res = engine.mediator().registerService<engine::IInputService>(_inputService); !res) {
        throw engine::MediatorError(res.error());
    }
}

void InputPlugin::preUpdate(engine::Engine& engine) {
    _inputSystem.update(engine.service<engine::IWindowService>(), engine.registry(), &engine.eventBus());
}

void InputPlugin::shutdown(engine::Engine& engine) {
    if (engine.mediator().hasService<engine::IInputService>()) {
        engine.mediator().removeService<engine::IInputService>();
    }
    _inputService.reset();
}

}  // namespace input
