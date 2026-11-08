#include "RosterPlugin.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "Arena.hpp"
#include "CarSimulationPlugin.hpp"
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
#include "log/Logger.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationSystems.hpp"
#include "protocol/Protocol.hpp"
#include "scripting/ScriptPlugin.hpp"

namespace rlss::server {

namespace {

constexpr const char* LOG_TAG = "match";
/** @brief Name of a player who has not sent one, followed by its slot number. */
constexpr const char* DEFAULT_NAME = "JOUEUR ";
}  // namespace

void RosterPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ecs::net::NetworkPlugin>();
    builder.require<CarSimulationPlugin>();
}

void RosterPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _connected = events.subscribe<ecs::net::PeerConnected>([this, &engine](const ecs::net::PeerConnected& event) {
        if (join(engine, event.peer)) {
            engine::log::info(LOG_TAG, "player ", event.peer, " joined (", _players.size(), " playing)");
        }
    });
    _disconnected =
        events.subscribe<ecs::net::PeerDisconnected>([this, &engine](const ecs::net::PeerDisconnected& event) {
            if (leave(engine, event.peer)) {
                engine::log::info(LOG_TAG, "player ", event.peer, " left (", _players.size(), " playing)");
            }
        });
    _packets = events.subscribe<ecs::net::PacketReceived>([this, &engine](const ecs::net::PacketReceived& packet) {
        const auto player = _players.find(packet.peer);
        if (player == _players.end()) {
            return;
        }
        const auto message = protocol::decode(packet.data);  // untrusted: a malformed name is dropped
        const auto* name = message ? std::get_if<protocol::SetName>(&*message) : nullptr;
        if (name != nullptr && name->name != player->second.name) {
            player->second.name = name->name;
            broadcastNames(engine);
        }
    });
}

void RosterPlugin::broadcastNames(engine::Engine& engine) const {
    protocol::PlayerNames names;
    for (const auto& [peer, player] : _players) {
        names.players.push_back(protocol::PlayerName{.carId = player.carId, .name = player.name});
    }
    engine.service<ecs::net::INetworkService>().broadcast(protocol::encode(names), ecs::net::Channel::Reliable);
}

void RosterPlugin::shutdown(engine::Engine& engine) {
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
    _players.clear();
    _slots = {};
}

std::uint8_t RosterPlugin::playersIn(const protocol::Team team) const noexcept {
    std::uint8_t count = 0;
    for (auto slot = static_cast<std::size_t>(team); slot < _slots.size(); slot += 2) {
        count += _slots.at(slot) ? 1 : 0;
    }
    return count;
}

std::optional<std::size_t> RosterPlugin::freeSlot() const {
    const protocol::Team team = playersIn(protocol::Team::Orange) < playersIn(protocol::Team::Blue)
                                    ? protocol::Team::Orange
                                    : protocol::Team::Blue;
    for (auto slot = static_cast<std::size_t>(team); slot < _slots.size(); slot += 2) {
        if (!_slots.at(slot)) {
            return slot;
        }
    }
    return std::nullopt;
}

bool RosterPlugin::join(engine::Engine& engine, const ecs::net::PeerId peer) {
    if (_players.contains(peer)) {
        return false;
    }
    const auto slot = freeSlot();
    if (!slot) {
        // The MatchServer never lets a fifth player in; refuse anyway rather than overflow the teams.
        engine::log::warning(LOG_TAG, "match full, player ", peer, " refused");
        return false;
    }
    _slots.at(*slot) = true;

    ecs::Registry& registry = engine.registry();
    const ecs::Entity car = registry.spawnEntity();
    // The `Car` script moves it to its kickoff spot (`placeAtKickoff`): Kickoff.wren owns the spots.
    registry.addComponent(car, ecs::Transform{.x = (arena::LEFT + arena::RIGHT) / 2.0F, .y = arena::BOTTOM / 2.0F});
    input::InputState controls;
    controls.playerIndex = static_cast<int>(*slot);
    registry.addComponent(car, std::move(controls));
    registry.addComponent(car, ecs::InputBuffer{});
    registry.addComponent(car, ecs::NetworkOwner{.client = peer});
    registry.addComponent(car, game::CarLook{.color = static_cast<std::uint8_t>(*slot % PLAYER_COLORS.size())});
    registry.addComponent(car, game::CarStatus{.boost = 0.0F, .boosting = false, .placeAtKickoff = true});
    registry.addComponent(car, game::CarCues{});
    // Stepped by the `Match` script, not by its own update.
    registry.addComponent(car, ecs::ScriptComponent{
                                   .moduleName = std::string(game::CAR_SCRIPT_MODULE),
                                   .className = "Car",
                               });
    const std::uint32_t carId = ecs::net::replicate(registry, car, game::KIND_CAR);
    // The client needs to know which car is its own (its camera follows it) and its team (who won).
    engine.service<ecs::net::INetworkService>().send(
        peer, protocol::encode(protocol::Welcome{.carId = carId, .team = static_cast<protocol::Team>(*slot % 2)}),
        ecs::net::Channel::Reliable);
    _players.emplace(peer, Player{
                               .slot = *slot,
                               .car = car,
                               .carId = carId,
                               .name = DEFAULT_NAME + std::to_string(*slot + 1),
                           });
    broadcastNames(engine);
    return true;
}

bool RosterPlugin::leave(engine::Engine& engine, const ecs::net::PeerId peer) {
    const auto it = _players.find(peer);
    if (it == _players.end()) {
        return false;
    }
    // Through the script system: it drops the script instance before the entity is killed.
    engine.getPlugin<scripting::ScriptPlugin>().scriptSystem().queueDestroyEntity(it->second.car);
    _slots.at(it->second.slot) = false;
    _players.erase(it);
    broadcastNames(engine);
    return true;
}

}  // namespace rlss::server
