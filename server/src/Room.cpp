#include "Room.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "components/InputBuffer.hpp"
#include "components/NetworkId.hpp"
#include "components/NetworkOwner.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"
#include "net/InputQueue.hpp"
#include "net/NetworkEntityMap.hpp"

namespace rtype::server {

Room::Room(const std::uint8_t maxPlayers, const float fixedDt, ecs::time::Clock::TimeSource timeSource)
    : _maxPlayers(maxPlayers), _fixedDt(fixedDt), _clock(std::move(timeSource)) {
    _registry.emplaceResource<ecs::net::NetworkEntityMap>();
    // TODO(ecs): once the ScriptSystem is used, give it this room's world: _scripts.init(_registry, _events);
}

bool Room::addPlayer(const ClientId player) {
    if (_playing || isFull() || _players.contains(player)) {
        return false;
    }
    const ecs::Entity entity = _registry.spawnEntity();
    const std::uint32_t netId = _registry.getResource<ecs::net::NetworkEntityMap>().assign(entity);
    _registry.emplaceComponent<ecs::NetworkId>(entity, netId);
    _registry.emplaceComponent<ecs::NetworkOwner>(entity, player);
    _registry.emplaceComponent<ecs::InputBuffer>(entity, ecs::InputBuffer{.capacity = MAX_PENDING_INPUTS});
    // TODO(ecs): give the player its gameplay components (create a spawn player in the ecs)
    _players.emplace(player, entity);
    return true;
}

void Room::removePlayer(const ClientId player) {
    const auto it = _players.find(player);
    if (it == _players.end()) {
        return;
    }
    _registry.getResource<ecs::net::NetworkEntityMap>().unbindEntity(it->second);
    _registry.killEntity(it->second);
    _players.erase(it);
}

void Room::onInput(const ClientId player, const PlayerInput& input) {
    const auto it = _players.find(player);
    if (it == _players.end()) {
        return;
    }
    ecs::net::pushInput(_registry.getComponents<ecs::InputBuffer>().get(it->second), input);
}

void Room::start() {
    _playing = true;
    _clock.reset();
}

std::size_t Room::advance() {
    if (!_playing) {
        return 0;
    }
    return _clock.tick(_fixedDt, _registry, &Room::step);
}

void Room::step(ecs::Registry& registry) {
    // One input per step, like the client prediction: the server stays at the same pace as the client.
    registry.runSystem<ecs::InputBuffer>([](ecs::Entity /*entity*/, ecs::InputBuffer& buffer) {
        // TODO(ecs): applyInput(registry, entity, *input), the same function as the client prediction, so that both
        // sides compute the same result.
        [[maybe_unused]] const auto input = ecs::net::popInput(buffer);
    });

    // TODO(ecs): run the systems, always in the same order, with the Time resource, e.g.:
    // const float dt = registry.getResource<ecs::time::Time>().deltaTime;
    // updateMovement(registry, dt);
    // updateCollisions(registry, _events);
    // _scripts.update(time.deltaTime);   // also flushes its CommandBuffer (spawns / kills requested by scripts)
}

std::uint32_t Room::lastProcessedInput(const ClientId player) const {
    const auto it = _players.find(player);
    if (it == _players.end()) {
        return 0;
    }
    return _registry.getComponents<ecs::InputBuffer>().get(it->second).lastProcessed;
}

}  // namespace rtype::server
