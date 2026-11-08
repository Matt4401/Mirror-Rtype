#include "PlayerInputPlugin.hpp"

#include <algorithm>
#include <utility>

#include "ClientEvents.hpp"
#include "components/InputBuffer.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "input/InputPlugin.hpp"
#include "input/InputState.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/ReplicationEvents.hpp"

namespace rlss::client {

namespace {

/** @brief `input::InputPlugin` samples the state whose `playerIndex` is 0 by default. */
constexpr int LOCAL_PLAYER = 0;

}  // namespace

void PlayerInputPlugin::build(engine::PluginBuilder& builder) {
    builder.require<input::InputPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
}

void PlayerInputPlugin::init(engine::Engine& engine) {
    input::InputState state;
    state.playerIndex = LOCAL_PLAYER;
    const ecs::Entity entity = engine.registry().spawnEntity();
    engine.registry().addComponent(entity, std::move(state));
    _joined = engine.eventBus().subscribe<MatchJoined>([this](const MatchJoined& /*event*/) { _inMatch = true; });
    _left = engine.eventBus().subscribe<MatchLeft>([this](const MatchLeft& /*event*/) { _inMatch = false; });
}

void PlayerInputPlugin::shutdown(engine::Engine& engine) {
    if (_joined) {
        engine.eventBus().unsubscribe<MatchJoined>(*_joined);
        _joined.reset();
    }
    if (_left) {
        engine.eventBus().unsubscribe<MatchLeft>(*_left);
        _left.reset();
    }
}

void PlayerInputPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    if (!_inMatch) {
        return;  // in the menu, the server has nothing to drive
    }
    ecs::NetInput input{.sequence = ++_sequence, .held = {}, .axes = {}};
    engine.registry().runSystem<input::InputState>([&input](ecs::Entity /*entity*/, const input::InputState& state) {
        if (state.playerIndex != LOCAL_PLAYER) {
            return;
        }
        input.axes.push_back(
            {.action = game::STEER_ID, .value = std::clamp(state.getAxis(game::STEER_ID), -1.0F, 1.0F)});
        input.axes.push_back(
            {.action = game::PITCH_ID, .value = std::clamp(state.getAxis(game::PITCH_ID), -1.0F, 1.0F)});
        if (state.isDown(game::BOOST_ID)) {
            input.held.push_back(game::BOOST_ID);
        }
        if (state.isDown(game::JUMP_ID)) {
            input.held.push_back(game::JUMP_ID);
        }
    });
    engine.eventBus().publish(ecs::net::LocalInput{.input = std::move(input)});
}

}  // namespace rlss::client
