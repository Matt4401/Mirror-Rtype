#include "CarSimulationPlugin.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
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
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "net/ServerReplicationPlugin.hpp"
#include "protocol/Protocol.hpp"
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
                                      .goalTop = arena::GOAL_TOP,
                                      .goalBottom = arena::GOAL_BOTTOM,
                                      .goalDepth = arena::GOAL_DEPTH,
                                  });
    registry.addComponent(_arena, game::MatchState{});
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

    // A player who joins gets the score at once; afterwards, `update` sends it when it changes.
    _connected =
        engine.eventBus().subscribe<ecs::net::PeerConnected>([this, &engine](const ecs::net::PeerConnected& event) {
            if (const auto status = currentStatus(engine)) {
                engine.service<ecs::net::INetworkService>().send(event.peer, protocol::encode(*status),
                                                                 ecs::net::Channel::Reliable);
            }
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
        const auto held = [&event](const input::ActionId action) {
            return std::ranges::find(event.input.held, action) != event.input.held.end();
        };
        state.setDown(game::JUMP_ID, held(game::JUMP_ID));
        state.setDown(game::BOOST_ID, held(game::BOOST_ID));
    });
}

void CarSimulationPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    const auto status = currentStatus(engine);
    if (!status || (_sent && *status == *_sent)) {
        return;
    }
    engine.service<ecs::net::INetworkService>().broadcast(protocol::encode(*status), ecs::net::Channel::Reliable);
    _sent = status;
}

std::optional<protocol::MatchStatus> CarSimulationPlugin::currentStatus(engine::Engine& engine) const {
    const auto& states = engine.registry().getComponents<game::MatchState>();
    if (!states.contains(_arena)) {
        return std::nullopt;
    }
    const game::MatchState& state = states.get(_arena);
    const auto score = [](const int goals) {
        return static_cast<std::uint16_t>(
            std::clamp(goals, 0, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
    };
    return protocol::MatchStatus{
        .blueScore = score(state.blueScore),
        .orangeScore = score(state.orangeScore),
        .phase = static_cast<protocol::MatchPhase>(std::clamp(state.phase, 0, 2)),
        .countdown =
            static_cast<std::uint8_t>(std::clamp(state.countdown, 0, static_cast<int>(protocol::MAX_COUNTDOWN))),
    };
}

void CarSimulationPlugin::shutdown(engine::Engine& engine) {
    if (_inputs) {
        engine.eventBus().unsubscribe<ecs::net::InputApplied>(*_inputs);
        _inputs.reset();
    }
    if (_connected) {
        engine.eventBus().unsubscribe<ecs::net::PeerConnected>(*_connected);
        _connected.reset();
    }
    for (const ecs::Entity entity : {_ball, _arena}) {
        if (engine.registry().isAlive(entity)) {
            engine.registry().killEntity(entity);
        }
    }
}

}  // namespace rlss::server
