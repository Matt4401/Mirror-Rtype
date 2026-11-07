#include "LobbyPlugin.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

#include "Arena.hpp"
#include "CarSimulationPlugin.hpp"
#include "Logger.hpp"
#include "components/InputBuffer.hpp"
#include "components/NetworkOwner.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "input/InputState.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationSystems.hpp"
#include "protocol/Protocol.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::server {

namespace {

constexpr const char* LOG_TAG = "lobby";
}  // namespace

void LobbyPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ecs::net::NetworkPlugin>();
    builder.require<CarSimulationPlugin>();
}

void LobbyPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _connected = events.subscribe<ecs::net::PeerConnected>([this, &engine](const ecs::net::PeerConnected& event) {
        join(engine, event.peer);
        rtype::log::info(LOG_TAG, "player ", event.peer, " joined (", _players.size(), " connected)");
        broadcastState(engine);
    });
    _disconnected =
        events.subscribe<ecs::net::PeerDisconnected>([this, &engine](const ecs::net::PeerDisconnected& event) {
            if (!leave(engine, event.peer)) {
                return;
            }
            rtype::log::info(LOG_TAG, "player ", event.peer, " left (", _players.size(), " connected)");
            broadcastState(engine);
        });
}

void LobbyPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_connected) {
        events.unsubscribe<ecs::net::PeerConnected>(*_connected);
        _connected.reset();
    }
    if (_disconnected) {
        events.unsubscribe<ecs::net::PeerDisconnected>(*_disconnected);
        _disconnected.reset();
    }
    _players.clear();
    _slots.clear();
}

void LobbyPlugin::join(engine::Engine& engine, const ecs::net::PeerId peer) {
    if (_players.contains(peer)) {
        return;
    }
    const auto freeSlot = std::ranges::find(_slots, false);
    const auto slot = static_cast<std::size_t>(std::distance(_slots.begin(), freeSlot));
    if (freeSlot == _slots.end()) {
        _slots.push_back(true);
    } else {
        *freeSlot = true;
    }

    ecs::Registry& registry = engine.registry();
    const ecs::Entity car = registry.spawnEntity();
    // The `Car` script moves it to its kickoff spot (`placeAtKickoff`): Kickoff.wren owns the spots.
    registry.addComponent(car, ecs::Transform{.x = (arena::LEFT + arena::RIGHT) / 2.0F, .y = arena::BOTTOM / 2.0F});
    input::InputState controls;
    controls.playerIndex = static_cast<int>(slot);
    registry.addComponent(car, std::move(controls));
    registry.addComponent(car, ecs::InputBuffer{});
    registry.addComponent(car, ecs::NetworkOwner{.client = peer});
    registry.addComponent(car, game::CarLook{.color = static_cast<std::uint8_t>(slot % PLAYER_COLORS.size())});
    registry.addComponent(car, game::CarStatus{.boost = 0.0F, .boosting = false, .placeAtKickoff = true});
    // Stepped by the `Match` script, not by its own update.
    registry.addComponent(car, ecs::ScriptComponent{
                                   .moduleName = std::string(game::CAR_SCRIPT_MODULE),
                                   .className = "Car",
                               });
    const std::uint32_t carId = ecs::net::replicate(registry, car, game::KIND_CAR);
    // The client needs to know which car is its own: its camera follows it.
    engine.service<ecs::net::INetworkService>().send(peer, protocol::encode(protocol::Welcome{.carId = carId}),
                                                     ecs::net::Channel::Reliable);
    _players.emplace(peer, Player{.slot = slot, .car = car});
}

bool LobbyPlugin::leave(engine::Engine& engine, const ecs::net::PeerId peer) {
    const auto it = _players.find(peer);
    if (it == _players.end()) {
        return false;
    }
    // Through the script system: it drops the script instance before the entity is killed.
    engine.getPlugin<scripting::ScriptPlugin>().scriptSystem().queueDestroyEntity(it->second.car);
    _slots.at(it->second.slot) = false;
    _players.erase(it);
    return true;
}

void LobbyPlugin::broadcastState(engine::Engine& engine) const {
    const auto players =
        static_cast<std::uint16_t>(std::min<std::size_t>(_players.size(), std::numeric_limits<std::uint16_t>::max()));
    engine.service<ecs::net::INetworkService>().broadcast(protocol::encode(protocol::LobbyState{.players = players}),
                                                          ecs::net::Channel::Reliable);
}

}  // namespace rlss::server
