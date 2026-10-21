#include "Room.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Clock.hpp"

namespace rtype::server {

Room::Room(const std::uint8_t maxPlayers, const float fixedDt, ecs::time::Clock::TimeSource timeSource)
    : _maxPlayers(maxPlayers), _fixedDt(fixedDt), _clock(std::move(timeSource)) {
    // TODO(ecs): once the ScriptSystem is used, give it this room's world: _scripts.init(_registry, _events);
}

bool Room::addPlayer(const ClientId player) {
    if (_playing || isFull() || _players.contains(player)) {
        return false;
    }
    const ecs::Entity entity = _registry.spawnEntity();
    // TODO(ecs): give the player its components (create a spawn player in the ecs)
    _players.emplace(player, Player{.entity = entity, .pendingInput = std::nullopt, .lastProcessedInput = 0});
    return true;
}

void Room::removePlayer(const ClientId player) {
    const auto it = _players.find(player);
    if (it == _players.end()) {
        return;
    }
    _registry.killEntity(it->second.entity);
    _players.erase(it);
}

void Room::onInput(const ClientId player, const PlayerInput& input) {
    const auto it = _players.find(player);
    if (it == _players.end()) {
        return;
    }
    Player& data = it->second;
    if (input.sequence <= data.lastProcessedInput ||
        (data.pendingInput && input.sequence <= data.pendingInput->sequence)) {
        return;
    }
    data.pendingInput = input;
}

void Room::start() {
    _playing = true;
    _clock.reset();
}

std::size_t Room::advance() {
    if (!_playing) {
        return 0;
    }
    return _clock.tick(_fixedDt, _registry, [this](const ecs::Registry& registry) { step(registry); });
}

void Room::step([[maybe_unused]] const ecs::Registry& registry) {
    for (auto& data : _players | std::views::values) {
        if (!data.pendingInput) {
            continue;
        }
        // TODO(ecs): applyInput(_registry, data.entity, *data.pendingInput), the same function as the client
        // prediction, so that both sides compute the same result.
        data.lastProcessedInput = data.pendingInput->sequence;
        data.pendingInput.reset();
    }

    // TODO(ecs): run the systems, always in the same order, with time.deltaTime, e.g.:
    // updateMovement(_registry, time.deltaTime);
    // updateCollisions(_registry, _events);
    // _scripts.update(time.deltaTime);   // also flushes its CommandBuffer (spawns / kills requested by scripts)
}

std::vector<ClientId> Room::players() const {
    std::vector<ClientId> ids;
    ids.reserve(_players.size());
    for (const auto& id : _players | std::views::keys) {
        ids.push_back(id);
    }
    return ids;
}

std::uint32_t Room::lastProcessedInput(const ClientId player) const {
    const auto it = _players.find(player);
    return it == _players.end() ? 0 : it->second.lastProcessedInput;
}

}  // namespace rtype::server
