#include "net/ReplicationPlugins.hpp"

#include <cstdint>
#include <memory>
#include <utility>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "net/SnapshotApplier.hpp"

namespace ecs::net {

// ---------------------------------------------------------------------------------------------------------------------
// Server

ServerReplicationPlugin::ServerReplicationPlugin(std::shared_ptr<IReplicationCodec> codec) : _codec(std::move(codec)) {}

void ServerReplicationPlugin::build(engine::PluginBuilder& builder) { builder.require<NetworkPlugin>(); }

void ServerReplicationPlugin::init(engine::Engine& engine) {
    _packets = engine.eventBus().subscribe<PacketReceived>([this, &engine](const PacketReceived& packet) {
        if (auto input = _codec->decodeInput(packet.data)) {
            receiveInput(engine.registry(), packet.peer, std::move(*input));
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
    sendSnapshots(engine.registry(), *_codec, engine.service<engine::INetworkService>(), *_pendingTick);
    _pendingTick.reset();
}

void ServerReplicationPlugin::shutdown(engine::Engine& engine) {
    if (_packets) {
        engine.eventBus().unsubscribe<PacketReceived>(*_packets);
        _packets.reset();
    }
    _pendingTick.reset();
}

// ---------------------------------------------------------------------------------------------------------------------
// Client

ClientReplicationPlugin::ClientReplicationPlugin(std::shared_ptr<IReplicationCodec> codec) : _codec(std::move(codec)) {}

void ClientReplicationPlugin::build(engine::PluginBuilder& builder) { builder.require<NetworkPlugin>(); }

void ClientReplicationPlugin::init(engine::Engine& engine) {
    auto& registry = engine.registry();
    if (!registry.hasResource<SnapshotState>()) {
        registry.emplaceResource<SnapshotState>();
    }
    _packets = engine.eventBus().subscribe<PacketReceived>(
        [this, &engine](const PacketReceived& packet) { onPacket(engine, packet); });
    _inputs = engine.eventBus().subscribe<LocalInput>([this, &engine](const LocalInput& local) {
        engine.service<engine::INetworkService>().send(SERVER_PEER, _codec->encodeInput(local.input),
                                                       Channel::Unreliable);
        predict(engine.registry(), engine.eventBus(), local.input, engine.fixedDeltaTime());
    });
}

void ClientReplicationPlugin::onPacket(engine::Engine& engine, const PacketReceived& packet) {
    const auto header = _codec->decodeSnapshot(packet.data);
    if (!header) {
        return;
    }
    auto& registry = engine.registry();
    auto& state = registry.getResource<SnapshotState>();
    // Unreliable channel: an older snapshot can arrive after a newer one; applying it would move entities back.
    if (state.tick && header->tick <= *state.tick) {
        return;
    }
    const float interval = state.tick ? static_cast<float>(header->tick - *state.tick) * engine.fixedDeltaTime() : 0.0F;

    SnapshotApplier applier(registry, registry.getResource<NetworkEntityMap>(), interval);
    _codec->applySnapshot(applier);
    state.tick = header->tick;
    state.lastProcessedInput = header->lastProcessedInput;

    for (const auto& [entity, id, kind] : applier.spawned()) {
        engine.eventBus().publish(ReplicatedEntitySpawned{.entity = entity, .id = id, .kind = kind});
    }
    despawnMissing(registry, engine.eventBus(), applier);
    reconcile(registry, engine.eventBus(), applier, header->lastProcessedInput, engine.fixedDeltaTime());
}

void ClientReplicationPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    interpolate(engine.registry(), time.deltaTime);
}

void ClientReplicationPlugin::shutdown(engine::Engine& engine) {
    if (_packets) {
        engine.eventBus().unsubscribe<PacketReceived>(*_packets);
        _packets.reset();
    }
    if (_inputs) {
        engine.eventBus().unsubscribe<LocalInput>(*_inputs);
        _inputs.reset();
    }
}

}  // namespace ecs::net
