#include "CarSimulationPlugin.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "Arena.hpp"
#include "components/InputBuffer.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Tag.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "game/ScriptBindings.hpp"
#include "input/ActionId.hpp"
#include "input/InputState.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
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
    // The match script steps every car and the ball together.
    registry.addComponent(_arena, ecs::ScriptComponent{
                                      .moduleName = std::string(game::MATCH_SCRIPT_MODULE),
                                      .className = "Match",
                                      .hasUpdate = true,
                                  });

    // The ball starts in the middle of the arena and falls.
    _ball = registry.spawnEntity();
    registry.addComponent(_ball, ecs::Transform{
                                     .x = (arena::LEFT + arena::RIGHT) / 2.0F,
                                     .y = (arena::TOP + arena::BOTTOM) / 2.0F,
                                 });
    registry.addComponent(_ball, ecs::ScriptComponent{
                                     .moduleName = std::string(game::BALL_SCRIPT_MODULE),
                                     .className = "Ball",
                                 });
    ecs::net::replicate(registry, _ball, game::KIND_BALL);

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
        const auto held = [&event](const input::ActionId action) {
            return std::ranges::find(event.input.held, action) != event.input.held.end();
        };
        state.setDown(game::JUMP_ID, held(game::JUMP_ID));
        state.setDown(game::BOOST_ID, held(game::BOOST_ID));
    });
}

void CarSimulationPlugin::shutdown(engine::Engine& engine) {
    if (_inputs) {
        engine.eventBus().unsubscribe<ecs::net::InputApplied>(*_inputs);
        _inputs.reset();
    }
    for (const ecs::Entity entity : {_ball, _arena}) {
        if (engine.registry().isAlive(entity)) {
            engine.registry().killEntity(entity);
        }
    }
}

}  // namespace rlss::server
