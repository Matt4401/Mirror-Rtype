#include "Room.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "components/InputBuffer.hpp"
#include "components/NetworkOwner.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PeerGroupConnection.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationPlugins.hpp"
#include "net/ReplicationSystems.hpp"

namespace rtype::server {

namespace {

constexpr float SPAWN_X = 200.0F;
constexpr float SPAWN_FIRST_Y = 120.0F;
constexpr float SPAWN_SPACING_Y = 160.0F;

}  // namespace

Room::Room(ecs::net::INetworkServer& network, const std::uint8_t maxPlayers, const float fixedDt,
           ecs::time::Clock::TimeSource timeSource)
    : _maxPlayers(maxPlayers),
      _connection(std::make_shared<engine::PeerGroupConnection>(network)),
      _engine(std::move(timeSource)) {
    _engine.setFixedDeltaTime(fixedDt);
    _engine.addPlugin<ecs::net::NetworkPlugin>(_connection, _connection);
    _engine.addPlugin<ecs::net::ServerReplicationPlugin>(std::make_shared<game::ReplicationCodec>());
    // TODO(ecs): add the gameplay plugins (PhysicsPlugin, ScriptPlugin, shooting, collisions...).
    _engine.eventBus().subscribe<ecs::net::InputApplied>([this](const ecs::net::InputApplied& event) {
        ecs::Registry& registry = _engine.registry();
        if (registry.hasComponent<ecs::Transform>(event.entity)) {
            game::applyShipInput(registry.getComponents<ecs::Transform>().get(event.entity), event.input,
                                 event.deltaTime);
        }
    });
    _engine.init();
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
    // NetworkPlugin releases the network id of a killed entity.
    _engine.registry().killEntity(it->entity);
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
        auto& transform = registry.getComponents<ecs::Transform>().get(player.entity);
        transform.x = SPAWN_X;
        transform.y = SPAWN_FIRST_Y + (SPAWN_SPACING_Y * static_cast<float>(index));
        ++index;
    }
    _playing = true;
    _engine.clock().reset();
}

std::size_t Room::advance() { return _playing ? _engine.advance() : 0; }

std::uint32_t Room::lastProcessedInput(const ClientId player) const {
    const auto it = std::ranges::find(_players, player, &RoomPlayer::client);
    if (it == _players.end()) {
        return 0;
    }
    return registry().getComponents<ecs::InputBuffer>().get(it->entity).lastProcessed;
}

}  // namespace rtype::server
