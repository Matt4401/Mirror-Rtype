#include "Room.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "components/InputBuffer.hpp"
#include "components/NetworkOwner.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "events/EventBus.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "gameplay/GameplayPlugin.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PeerGroupConnection.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "net/ServerReplicationPlugin.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"
#include "systems/physics/PhysicsPlugin.hpp"

namespace rtype::server {

namespace {

// Sleep between two iterations of a room's thread. Short enough to keep the step jitter around 1 ms, long enough not to
// burn a core.
constexpr auto IDLE_SLEEP = std::chrono::milliseconds(1);

/**
 * @brief Sends a message to every player of a room on the reliable channel (TCP).
 *
 * Only for session events: level start and end, game over. They come a few times per game and must not be lost, like
 * `GameStart`. Nothing of the game loop goes there: snapshots and inputs stay on UDP.
 * @param connection Peers of the room.
 * @param message Message to send.
 */
void broadcastReliable(ecs::net::PeerGroupConnection& connection, const protocol::Message& message) {
    connection.broadcast(protocol::encode(message), ecs::net::Channel::Reliable);
}

}  // namespace

Room::Room(ecs::net::INetworkServer& network, const std::uint8_t maxPlayers, const float fixedDt,
           ecs::time::Clock::TimeSource timeSource)
    : _maxPlayers(maxPlayers),
      _connection(std::make_shared<ecs::net::PeerGroupConnection>(network)),
      _engine(std::move(timeSource)) {
    _engine.setFixedDeltaTime(fixedDt);
    _engine.addPlugin<ecs::net::NetworkPlugin>(_connection, _connection);
    _engine.addPlugin<ecs::net::ServerReplicationPlugin>(std::make_shared<game::ReplicationCodec>());
    _engine.addPlugin<systems::PhysicsPlugin>();
    _engine.addPlugin<gameplay::GameplayPlugin>();
    // TODO(ecs): add ScriptPlugin for the scripted enemies.
    ecs::EventBus& bus = _engine.eventBus();
    bus.subscribe<protocol::LevelStart>(
        [this](const protocol::LevelStart& message) { broadcastReliable(*_connection, message); });
    bus.subscribe<protocol::LevelComplete>(
        [this](const protocol::LevelComplete& message) { broadcastReliable(*_connection, message); });
    bus.subscribe<protocol::GameOver>([this](const protocol::GameOver& message) {
        broadcastReliable(*_connection, message);
        _ended = true;
    });
    _engine.eventBus().subscribe<ecs::net::InputApplied>([this](const ecs::net::InputApplied& event) {
        ecs::Registry& registry = _engine.registry();
        if (registry.hasComponent<ecs::Transform>(event.entity)) {
            game::applyShipInput(registry.getComponents<ecs::Transform>().get(event.entity), event.input,
                                 event.deltaTime);
        }
    });
    _engine.init();
}

Room::~Room() {
    _stopping = true;
    if (_thread.joinable()) {
        _thread.join();
    }
}

bool Room::addPlayer(const ClientId player) {
    if (_playing || isFull() || std::ranges::contains(_players, player, &RoomPlayer::client)) {
        return false;
    }
    ecs::Registry& registry = _engine.registry();
    const ecs::Entity entity = registry.spawnEntity();
    ecs::net::replicate(registry, entity, game::KIND_PLAYER_SHIP);
    registry.addComponent(entity, ecs::NetworkOwner{player});
    registry.addComponent(entity, ecs::InputBuffer{.capacity = MAX_PENDING_INPUTS});
    registry.addComponent(entity, game::EntityNetState{});
    registry.addComponent(entity, ecs::Transform{});
    _connection->addPeer(player);
    _players.push_back({.client = player, .entity = entity});
    return true;
}

void Room::removePlayer(const ClientId player) {
    const auto it = std::ranges::find(_players, player, &RoomPlayer::client);
    if (it == _players.end()) {
        return;
    }
    _connection->removePeer(player);
    {
        const std::scoped_lock lock(_mutex);
        // NetworkPlugin releases the network id of a killed entity.
        _engine.registry().killEntity(it->entity);
    }
    _players.erase(it);
}

void Room::setReady(const ClientId player, const bool ready) {
    const auto it = std::ranges::find(_players, player, &RoomPlayer::client);
    if (!_playing && it != _players.end()) {
        it->ready = ready;
    }
}

bool Room::isEveryoneReady() const { return !_players.empty() && std::ranges::all_of(_players, &RoomPlayer::ready); }

void Room::onPacket(const ClientId player, const ecs::net::Bytes& data) {
    if (!_playing) {
        return;
    }
    _connection->push(
        {.type = ecs::net::EventType::Data, .peer = player, .data = data, .channel = ecs::net::Channel::Unreliable});
}

void Room::start() {
    ecs::Registry& registry = _engine.registry();
    std::uint8_t index = 0;
    for (const RoomPlayer& player : _players) {
        registry.getComponents<game::EntityNetState>().get(player.entity).subtype = index;
        ++index;
    }
    auto& gameplay = _engine.getPlugin<gameplay::GameplayPlugin>();
    gameplay.setPlayerCount(_players.size());
    gameplay.placeShips();
    gameplay.startGame();
    _playing = true;
    _engine.clock().reset();
}

std::size_t Room::advance() {
    if (!_playing) {
        return 0;
    }
    const std::scoped_lock lock(_mutex);
    return _engine.advance();
}

void Room::runInBackground() {
    _thread = std::thread([this] {
        try {
            while (!_stopping.load()) {
                advance();
                std::this_thread::sleep_for(IDLE_SLEEP);
            }
        } catch (const std::exception& error) {
            log::error(LOG_TAG, "room stopped by an error in its simulation: ", error.what());
            broadcastReliable(*_connection, protocol::GameOver{.reason = protocol::GameOverReason::ServerShutdown});
            _ended = true;
        }
    });
}

std::uint32_t Room::lastProcessedInput(const ClientId player) const {
    const auto it = std::ranges::find(_players, player, &RoomPlayer::client);
    if (it == _players.end()) {
        return 0;
    }
    const std::scoped_lock lock(_mutex);
    return registry().getComponents<ecs::InputBuffer>().get(it->entity).lastProcessed;
}

}  // namespace rtype::server
