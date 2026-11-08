#include "net/AutoLobbyPlugin.hpp"

#include <algorithm>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/time/Time.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "net/LocalInput.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/SessionEvents.hpp"
#include "protocol/Messages.hpp"

namespace client {

namespace protocol = rtype::protocol;

void AutoLobbyPlugin::build(engine::PluginBuilder& builder) { builder.require<RTypeNetworkPlugin>(); }

void AutoLobbyPlugin::init(engine::Engine& engine) {
    auto& bus = engine.eventBus();
    bus.subscribe<RoomListUpdated>([this, &engine](const RoomListUpdated& event) {
        if (engine.getPlugin<RTypeNetworkPlugin>().state() != SessionState::Lobby) {
            return;
        }
        _ready = false;
        const auto open = std::ranges::find_if(event.list.rooms, [](const protocol::RoomEntry& room) {
            return room.state == protocol::RoomState::Waiting && room.playerCount < room.maxPlayers;
        });
        if (open == event.list.rooms.end()) {
            engine.eventBus().publish(CreateRoomRequest{});
        } else {
            engine.eventBus().publish(JoinRoomRequest{.roomId = open->roomId});
        }
    });
    bus.subscribe<SessionClosed>([this, &engine](const SessionClosed& /*event*/) {
        _failed = true;
        engine.stop();
    });
    bus.publish(ConnectRequest{.host = _options.host, .port = _options.port});
}

void AutoLobbyPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    if (engine.getPlugin<RTypeNetworkPlugin>().state() != SessionState::InRoom) {
        return;
    }
    bool pressed = false;
    withLocalInput(engine.registry(), [&pressed](const ::input::InputState& state) {
        pressed = state.isJustPressed(input::ACTION_READY);
    });
    if (pressed) {
        _ready = !_ready;
        engine.eventBus().publish(SetReadyRequest{.ready = _ready});
    }
}

}  // namespace client
