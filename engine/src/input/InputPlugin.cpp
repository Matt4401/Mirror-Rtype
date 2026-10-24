/**
 * @file InputPlugin.cpp
 * @brief Implementation of InputPlugin and its abstract service adapter.
 */

#include "input/InputPlugin.hpp"

#include <utility>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/services/IInputService.hpp"
#include "render/RenderPlugin.hpp"
#include "systems/input/InputSystem.hpp"

namespace input {

class InputPlugin::InputServiceImpl final : public engine::IInputService {
  public:
    explicit InputServiceImpl(systems::InputSystem& system) : _system(&system) {}

    [[nodiscard]] ActionMapping& mapping() noexcept override { return _system->mapping(); }

    [[nodiscard]] const ActionMapping& mapping() const noexcept override { return _system->mapping(); }

    [[nodiscard]] int playerIndex() const noexcept override { return _system->playerIndex(); }

    [[nodiscard]] int gamepadIndex() const noexcept override { return _system->gamepadIndex(); }

  private:
    systems::InputSystem* _system;
};

InputPlugin::InputPlugin(ActionMapping mapping, int playerIndex, int gamepadIndex)
    : _inputSystem(std::move(mapping), playerIndex, gamepadIndex) {}

InputPlugin::~InputPlugin() = default;

InputPlugin::InputPlugin(InputPlugin&&) noexcept = default;
InputPlugin& InputPlugin::operator=(InputPlugin&&) noexcept = default;

void InputPlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
    builder.require<render::RenderPlugin>();
}

void InputPlugin::init(engine::Engine& engine) {
    _inputService = std::make_shared<InputServiceImpl>(_inputSystem);
    engine.mediator().registerService<engine::IInputService>(_inputService);
}

void InputPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    _inputSystem.update(engine.registry(), &engine.eventBus());
}

void InputPlugin::shutdown(engine::Engine& engine) {
    if (engine.mediator().hasService<engine::IInputService>()) {
        engine.mediator().removeService<engine::IInputService>();
    }
    _inputService.reset();
}

}  // namespace input
