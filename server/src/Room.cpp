#include "Room.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <ranges>
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
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PeerGroupConnection.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "net/ServerReplicationPlugin.hpp"

namespace rtype::server {

namespace {

constexpr float SPAWN_X = 200.0F;
constexpr float SPAWN_FIRST_Y = 120.0F;
constexpr float SPAWN_SPACING_Y = 160.0F;
// Sleep between two iterations of a room's thread. Short enough to keep the step jitter around 1 ms, long enough not to
// burn a core.
constexpr auto IDLE_SLEEP = std::chrono::milliseconds(1);

}  // namespace

Room::Room(ecs::net::INetworkServer& network, const std::uint8_t maxPlayers, const float fixedDt,
           ecs::time::Clock::TimeSource timeSource)
    : _maxPlayers(maxPlayers),
      _connection(std::make_shared<ecs::net::PeerGroupConnection>(network)),
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

Room::~Room() {
    _stopping = true;
    if (_thread.joinable()) {
        _thread.join();
    }
}

bool Room::addPlayer(const ClientId player) {
    if (_playing || isFull() || _players.contains(player)) {
        return false;
    }
    ecs::Registry& registry = _engine.registry();
    const std::uint8_t index = freePlayerIndex();
    const ecs::Entity entity = registry.spawnEntity();
    ecs::net::replicate(registry, entity, game::KIND_PLAYER_SHIP);
    registry.addComponent(entity, ecs::NetworkOwner{player});
    registry.addComponent(entity, ecs::InputBuffer{.capacity = MAX_PENDING_INPUTS});
    registry.addComponent(entity, game::EntityNetState{.subtype = index});
    registry.addComponent(
        entity, ecs::Transform{.x = SPAWN_X, .y = SPAWN_FIRST_Y + (SPAWN_SPACING_Y * static_cast<float>(index))});
    _connection->addPeer(player);
    _players.emplace(player, entity);
    return true;
}

void Room::removePlayer(const ClientId player) {
    const auto it = _players.find(player);
    if (it == _players.end()) {
        return;
    }
    _connection->removePeer(player);
    {
        const std::scoped_lock lock(_mutex);
        // NetworkPlugin releases the network id of a killed entity.
        _engine.registry().killEntity(it->second);
    }
    _players.erase(it);
}

void Room::onPacket(const ClientId player, const ecs::net::Bytes& data) {
    if (!_playing) {
        return;
    }
    _connection->push(
        {.type = ecs::net::EventType::Data, .peer = player, .data = data, .channel = ecs::net::Channel::Unreliable});
}

void Room::start() {
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
        }
    });
}

std::uint32_t Room::lastProcessedInput(const ClientId player) const {
    const auto it = _players.find(player);
    if (it == _players.end()) {
        return 0;
    }
    const std::scoped_lock lock(_mutex);
    return registry().getComponents<ecs::InputBuffer>().get(it->second).lastProcessed;
}

std::uint8_t Room::freePlayerIndex() const {
    std::uint8_t index = 0;
    const auto isTaken = [&](const ecs::Entity entity) {
        return registry().getComponents<game::EntityNetState>().get(entity).subtype == index;
    };
    while (std::ranges::any_of(_players | std::views::values, isTaken)) {
        ++index;
    }
    return index;
}

}  // namespace rtype::server
