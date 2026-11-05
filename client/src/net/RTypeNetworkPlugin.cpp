#include "net/RTypeNetworkPlugin.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <variant>

#include "Logger.hpp"
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
#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationPlugins.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace client {

namespace log = rtype::log;
namespace protocol = rtype::protocol;

namespace {

constexpr const char* LOG_TAG = "client";
/** @brief The client has one local player. */
constexpr int LOCAL_PLAYER = 0;

/**
 * @brief Runs `action` on the input state of the local player, or does nothing if the game has none.
 * @param registry Client world.
 * @param action Callable taking the `input::InputState`.
 */
template <typename Action>
void withLocalInput(ecs::Registry& registry, Action action) {
    registry.runSystem<::input::InputState>([&](ecs::Entity /*entity*/, const ::input::InputState& state) {
        if (state.playerIndex == LOCAL_PLAYER) {
            action(state);
        }
    });
}

}  // namespace

RTypeNetworkPlugin::RTypeNetworkPlugin(std::shared_ptr<ecs::net::INetworkClient> transport)
    : _transport(std::move(transport)) {}

void RTypeNetworkPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ecs::net::NetworkPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
}

void RTypeNetworkPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _connected = events.subscribe<ecs::net::PeerConnected>(
        [&engine](const ecs::net::PeerConnected& /*event*/) { sendMessage(engine, protocol::Hello{}); });
    _disconnected =
        events.subscribe<ecs::net::PeerDisconnected>([&engine](const ecs::net::PeerDisconnected& /*event*/) {
            log::error(LOG_TAG, "connection to the server lost");
            engine.stop();
        });
    _packets = events.subscribe<ecs::net::PacketReceived>(
        [this, &engine](const ecs::net::PacketReceived& packet) { onPacket(engine, packet); });
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

void RTypeNetworkPlugin::onPacket(engine::Engine& engine, const ecs::net::PacketReceived& packet) {
    if (packet.channel != ecs::net::Channel::Reliable) {
        return;
    }
    const auto message = protocol::decode(packet.data);
    if (!message) {
        return;
    }
    if (const auto* welcome = std::get_if<protocol::Welcome>(&*message)) {
        _udpPort = welcome->udpPort;
        sendMessage(engine, protocol::LobbyListRequest{});
    } else if (const auto* list = std::get_if<protocol::LobbyList>(&*message)) {
        chooseRoom(engine, *list);
    } else if (const auto* update = std::get_if<protocol::LobbyUpdate>(&*message)) {
        if (!_room) {
            log::info(LOG_TAG, "in room ", update->roomId, ": press the ready key (R) when everyone is here");
        }
        _room = update->roomId;
        const auto ready = std::ranges::count_if(update->slots, &protocol::PlayerSlot::ready);
        log::info(LOG_TAG, "room ", update->roomId, ": ", ready, '/', update->slots.size(), " players ready");
    } else if (const auto* start = std::get_if<protocol::GameStart>(&*message)) {
        log::info(LOG_TAG, "game started: player ", static_cast<int>(start->playerIndex) + 1, " of ",
                  static_cast<int>(start->playerCount));
        _gameStart = *start;
        if (!_transport->openUdp(_udpPort)) {
            log::error(LOG_TAG, "cannot open UDP on port ", _udpPort);
        }
    } else if (const auto* left = std::get_if<protocol::PlayerLeft>(&*message)) {
        log::info(LOG_TAG, "player ", left->clientId, " left");
    }
}

void RTypeNetworkPlugin::chooseRoom(engine::Engine& engine, const protocol::LobbyList& list) const {
    if (_room) {
        return;
    }
    const auto open = std::ranges::find_if(list.rooms, [](const protocol::RoomEntry& room) {
        return room.state == protocol::RoomState::Waiting && room.playerCount < room.maxPlayers;
    });
    if (open == list.rooms.end()) {
        sendMessage(engine, protocol::LobbyCreate{});
    } else {
        sendMessage(engine, protocol::LobbyJoin{.roomId = open->roomId});
    }
}

void RTypeNetworkPlugin::onEntitySpawned(engine::Engine& engine, const ecs::net::ReplicatedEntitySpawned& event) const {
    ecs::Registry& registry = engine.registry();
    const bool isLocalShip =
        _gameStart && event.kind == rtype::game::KIND_PLAYER_SHIP &&
        registry.getComponents<rtype::game::EntityNetState>().get(event.entity).subtype == _gameStart->playerIndex;
    if (isLocalShip) {
        registry.addComponent(event.entity, ecs::Predicted{});
    } else {
        registry.addComponent(event.entity, ecs::Interpolated{});
    }
}

void RTypeNetworkPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    if (!_gameStart) {
        return;
    }
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

void RTypeNetworkPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    if (!_room || _gameStart) {
        return;
    }
    bool pressed = false;
    withLocalInput(engine.registry(), [&pressed](const ::input::InputState& state) {
        pressed = state.isJustPressed(input::ACTION_READY);
    });
    if (pressed) {
        _ready = !_ready;
        sendMessage(engine, protocol::LobbyReady{.ready = _ready});
    }
}

void RTypeNetworkPlugin::shutdown(engine::Engine& engine) {
    // The service is still there: NetworkPlugin, a dependency, shuts down after this plugin.
    sendMessage(engine, protocol::Leave{});
    auto& events = engine.eventBus();
    if (_connected) {
        events.unsubscribe<ecs::net::PeerConnected>(*_connected);
        _connected.reset();
    }
    if (_disconnected) {
        events.unsubscribe<ecs::net::PeerDisconnected>(*_disconnected);
        _disconnected.reset();
    }
    if (_packets) {
        events.unsubscribe<ecs::net::PacketReceived>(*_packets);
        _packets.reset();
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

void RTypeNetworkPlugin::sendMessage(engine::Engine& engine, const protocol::Message& message) {
    engine.service<engine::INetworkService>().send(ecs::net::SERVER_PEER, protocol::encode(message),
                                                   ecs::net::Channel::Reliable);
}

}  // namespace client
