#include "CarSimulationPlugin.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "Arena.hpp"
#include "components/InputBuffer.hpp"
#include "components/Tag.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "game/ScriptBindings.hpp"
#include "input/InputState.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ServerReplicationPlugin.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::server {

void CarSimulationPlugin::build(engine::PluginBuilder& builder) {
    builder.require<scripting::ScriptPlugin>();
    builder.require<ecs::net::ServerReplicationPlugin>();
}

void CarSimulationPlugin::init(engine::Engine& engine) {
    if (!game::installScriptBindings(engine.getPlugin<scripting::ScriptPlugin>().scriptSystem())) {
        throw std::runtime_error("CarSimulationPlugin: Wren refused the RLSS component bindings");
    }

    ecs::Registry& registry = engine.registry();
    _arena = registry.spawnEntity();
    registry.addComponent(_arena, ecs::Tag{.name = std::string(game::ARENA_TAG)});
    registry.addComponent(_arena, game::ArenaShape{
                                      .left = arena::LEFT,
                                      .top = arena::TOP,
                                      .right = arena::RIGHT,
                                      .bottom = arena::BOTTOM,
                                      .cornerRadius = arena::CORNER_RADIUS,
                                  });

    _inputs = engine.eventBus().subscribe<ecs::net::InputApplied>([&registry](const ecs::net::InputApplied& event) {
        auto& states = registry.getComponents<input::InputState>();
        if (!states.contains(event.entity)) {
            return;
        }
        input::InputState& state = states.get(event.entity);
        float steer = 0.0F;
        for (const ecs::NetAxis& axis : event.input.axes) {
            if (axis.action == game::STEER_ID) {
                steer = std::clamp(axis.value, -1.0F, 1.0F);
            }
        }
        state.setAxis(game::STEER_ID, steer);
        state.setDown(game::JUMP_ID, std::ranges::find(event.input.held, game::JUMP_ID) != event.input.held.end());
    });
}

void CarSimulationPlugin::shutdown(engine::Engine& engine) {
    if (_inputs) {
        engine.eventBus().unsubscribe<ecs::net::InputApplied>(*_inputs);
        _inputs.reset();
    }
    if (engine.registry().isAlive(_arena)) {
        engine.registry().killEntity(_arena);
    }
}

}  // namespace rlss::server
