#include "tools/console/ConsolePlugin.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "events/EventBus.hpp"
#include "tools/console/BuiltinCommands.hpp"
#include "tools/console/Console.hpp"
#include "tools/console/ConsoleEvents.hpp"
#include "tools/cvars/CVarManager.hpp"

namespace tools {

ConsolePlugin::ConsolePlugin(ConsoleConfig config)
    : _cvars(std::make_shared<CVarManager>()), _console(std::make_shared<Console>(*_cvars)) {
    _cvars->setCheatsAllowed(config.allowCheats);
    registerBuiltinCommands(*_console);
}

ConsolePlugin::~ConsolePlugin() = default;

void ConsolePlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
    builder.provides<CVarManager>();
    builder.provides<Console>();
}

void ConsolePlugin::init(engine::Engine& engine) {
    if (!engine.mediator().registerService<CVarManager>(_cvars)) {
        throw std::logic_error("ConsolePlugin: a CVarManager service is already registered");
    }
    if (!engine.mediator().registerService<Console>(_console)) {
        engine.mediator().removeService<CVarManager>();
        throw std::logic_error("ConsolePlugin: a Console service is already registered");
    }
}

void ConsolePlugin::preUpdate(engine::Engine& engine) {
    ecs::EventBus& bus = engine.eventBus();
    _cvars->drainChanged([&bus](std::string_view name) { bus.publish(CVarChanged{.name = std::string(name)}); });
}

void ConsolePlugin::shutdown(engine::Engine& engine) {
    engine.mediator().removeService<Console>();
    engine.mediator().removeService<CVarManager>();
}

}  // namespace tools
