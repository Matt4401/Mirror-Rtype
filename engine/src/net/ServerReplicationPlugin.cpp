#include "net/ServerReplicationPlugin.hpp"

#include <cstdint>
#include <memory>
#include <utility>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/time/Time.hpp"
#include "net/INetworkService.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationSystems.hpp"

namespace ecs::net {

ServerReplicationPlugin::ServerReplicationPlugin(std::shared_ptr<IReplicationCodec> codec) : _codec(std::move(codec)) {}

void ServerReplicationPlugin::build(engine::PluginBuilder& builder) { builder.require<NetworkPlugin>(); }

void ServerReplicationPlugin::init(engine::Engine& engine) {
    _packets = engine.eventBus().subscribe<PacketReceived>([this, &engine](const PacketReceived& packet) {
        if (auto input = _codec->decodeInput(packet.data)) {
            receiveInput(engine.registry(), packet.peer, *input);
        }
    });
}

void ServerReplicationPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    applyInputs(engine.registry(), engine.eventBus(), time.deltaTime);
    _pendingTick = static_cast<std::uint32_t>(time.tickCount);
}

void ServerReplicationPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    // After several catch-up steps, one snapshot is enough: it carries the latest state.
    if (!_pendingTick) {
        return;
    }
    sendSnapshots(engine.registry(), *_codec, engine.service<INetworkService>(), *_pendingTick);
    _pendingTick.reset();
}

void ServerReplicationPlugin::shutdown(engine::Engine& engine) {
    if (_packets) {
        engine.eventBus().unsubscribe<PacketReceived>(*_packets);
        _packets.reset();
    }
    _pendingTick.reset();
}

}  // namespace ecs::net
