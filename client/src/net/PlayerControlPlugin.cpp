#include "net/PlayerControlPlugin.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "components/InputBuffer.hpp"
#include "components/Interpolated.hpp"
#include "components/Predicted.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "metrics/MetricsBoard.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/LocalInput.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/SessionEvents.hpp"
#include "protocol/Messages.hpp"

namespace client {

namespace protocol = rtype::protocol;

void PlayerControlPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ecs::net::ClientReplicationPlugin>();
    builder.require<RTypeNetworkPlugin>();
}

void PlayerControlPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _started = events.subscribe<GameStarted>([this](const GameStarted& event) { _playerIndex = event.playerIndex; });
    // UDP only opens at GameStart, so a snapshot never arrives before the index of this player's ship is known.
    _spawned = events.subscribe<ecs::net::ReplicatedEntitySpawned>(
        [this, &engine](const ecs::net::ReplicatedEntitySpawned& event) { onEntitySpawned(engine, event); });
    // Same movement as the server: the prediction and the replay after a snapshot compute what the server computes.
    _inputs = events.subscribe<ecs::net::InputApplied>([&engine](const ecs::net::InputApplied& event) {
        ecs::Registry& registry = engine.registry();
        if (registry.hasComponent<ecs::Transform>(event.entity)) {
            rtype::game::applyShipInput(registry.getComponents<ecs::Transform>().get(event.entity), event.input,
                                        event.deltaTime);
        }
    });
}

void PlayerControlPlugin::onEntitySpawned(engine::Engine& engine,
                                          const ecs::net::ReplicatedEntitySpawned& event) const {
    ecs::Registry& registry = engine.registry();
    const bool isLocalShip =
        _playerIndex && event.kind == rtype::game::KIND_PLAYER_SHIP &&
        registry.getComponents<rtype::game::EntityNetState>().get(event.entity).subtype == *_playerIndex;
    if (isLocalShip) {
        registry.addComponent(event.entity, ecs::Predicted{});
    } else {
        registry.addComponent(event.entity, ecs::Interpolated{});
    }
}

void PlayerControlPlugin::publishPosition(ecs::Registry& registry, const float deltaTime) {
    if (!_positionRefresh.tick(deltaTime) || !registry.hasResource<metrics::MetricsBoard>()) {
        return;
    }
    auto& board = registry.getResource<metrics::MetricsBoard>();
    registry.runSystem<ecs::Predicted, ecs::Transform>(
        [&board](const ecs::Entity /*entity*/, const ecs::Predicted& /*predicted*/, const ecs::Transform& transform) {
            board.set("Position", std::format("{:.0f}, {:.0f}", transform.x, transform.y));
        });
}

void PlayerControlPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    if (!_playerIndex) {
        return;
    }
    publishPosition(engine.registry(), time.deltaTime);
    ecs::NetInput input{.sequence = ++_sequence, .held = {}, .axes = {}};
    withLocalInput(engine.registry(), [&input](const ::input::InputState& state) {
        for (const auto& [action, data] : state.actions) {
            if (action == input::ACTION_READY) {
                continue;
            }
            if (data.held && input.held.size() < protocol::MAX_INPUT_BUTTONS) {
                input.held.push_back(action);
            }
            if (data.axis != 0.0F && input.axes.size() < protocol::MAX_INPUT_AXES) {
                input.axes.push_back({.action = action, .value = std::clamp(data.axis, -1.0F, 1.0F)});
            }
        }
    });
    engine.eventBus().publish(ecs::net::LocalInput{.input = std::move(input)});
}

void PlayerControlPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_started) {
        events.unsubscribe<GameStarted>(*_started);
        _started.reset();
    }
    if (_spawned) {
        events.unsubscribe<ecs::net::ReplicatedEntitySpawned>(*_spawned);
        _spawned.reset();
    }
    if (_inputs) {
        events.unsubscribe<ecs::net::InputApplied>(*_inputs);
        _inputs.reset();
    }
}

}  // namespace client
