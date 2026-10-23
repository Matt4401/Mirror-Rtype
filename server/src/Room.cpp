#include "Room.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <utility>

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
    _players.emplace(player, Player{.entity = entity, .pendingInputs = {}, .lastProcessedInput = 0});
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
    if (input.sequence <= data.lastProcessedInput) {
        return;
    }
    auto& queue = data.pendingInputs;
    const auto pos = std::ranges::upper_bound(queue, input.sequence, {}, &PlayerInput::sequence);
    if (pos != queue.begin() && std::prev(pos)->sequence == input.sequence) {
        return;  // duplicate
    }
    queue.insert(pos, input);
    if (queue.size() > MAX_PENDING_INPUTS) {
        queue.pop_front();  // drop the oldest
    }
}

void Room::start() {
    _playing = true;
    _clock.reset();
}

std::size_t Room::advance() {
    if (!_playing) {
        return 0;
    }
    return _clock.tick(_fixedDt, _registry, [this](ecs::Registry& registry) { step(registry); });
}

void Room::step([[maybe_unused]] ecs::Registry& registry) {
    // One input per step, like the client prediction: the server stays at the same pace as the client.
    for (auto& data : _players | std::views::values) {
        if (data.pendingInputs.empty()) {
            continue;
        }
        // TODO(ecs): applyInput(registry, data.entity, data.pendingInputs.front()), the same function as the client
        // prediction, so that both sides compute the same result.
        data.lastProcessedInput = data.pendingInputs.front().sequence;
        data.pendingInputs.pop_front();
    }

    // TODO(ecs): run the systems, always in the same order, with the Time resource, e.g.:
    // const float dt = registry.getResource<ecs::time::Time>().deltaTime;
    // updateMovement(registry, dt);
    // updateCollisions(registry, _events);
    // _scripts.update(time.deltaTime);   // also flushes its CommandBuffer (spawns / kills requested by scripts)
}

std::uint32_t Room::lastProcessedInput(const ClientId player) const {
    const auto it = _players.find(player);
    return it == _players.end() ? 0 : it->second.lastProcessedInput;
}

}  // namespace rtype::server
