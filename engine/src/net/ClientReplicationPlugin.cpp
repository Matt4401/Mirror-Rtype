#include "net/ClientReplicationPlugin.hpp"

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

ClientReplicationPlugin::ClientReplicationPlugin(std::shared_ptr<IReplicationCodec> codec) : _codec(std::move(codec)) {}

void ClientReplicationPlugin::build(engine::PluginBuilder& builder) { builder.require<NetworkPlugin>(); }

void ClientReplicationPlugin::init(engine::Engine& engine) {
    auto& registry = engine.registry();
    if (!registry.hasResource<SnapshotState>()) {
        registry.emplaceResource<SnapshotState>();
    }
    _packets = engine.eventBus().subscribe<PacketReceived>(
        [this, &engine](const PacketReceived& packet) { onPacket(engine, packet); });
    _inputs = engine.eventBus().subscribe<LocalInput>(
        [this, &engine](const LocalInput& local) { onLocalInput(engine, local); });
}

void ClientReplicationPlugin::onLocalInput(engine::Engine& engine, const LocalInput& local) {
    auto& service = engine.service<INetworkService>();
    const std::uint32_t acknowledged = engine.registry().getResource<SnapshotState>().lastProcessedInput;
    for (const SentInput& sent : _sentInputs) {
        if (sent.sequence > acknowledged) {
            service.send(SERVER_PEER, sent.bytes, Channel::Unreliable);
        }
    }
    Bytes bytes = _codec->encodeInput(local.input);
    service.send(SERVER_PEER, bytes, Channel::Unreliable);

    _sentInputs.push_back({.sequence = local.input.sequence, .bytes = std::move(bytes)});
    if (_sentInputs.size() > INPUT_RESEND_COUNT) {
        _sentInputs.pop_front();
    }
    predict(engine.registry(), engine.eventBus(), local.input, engine.fixedDeltaTime());
}

void ClientReplicationPlugin::onPacket(engine::Engine& engine, const PacketReceived& packet) {
    const auto header = _codec->decodeSnapshot(packet.data);
    if (!header || header->partCount == 0 || header->partCount > MAX_SNAPSHOT_PARTS ||
        header->partIndex >= header->partCount) {
        return;
    }
    auto& registry = engine.registry();
    auto& state = registry.getResource<SnapshotState>();
    // Unreliable channel: an older snapshot can arrive after a newer one; applying it would move entities back.
    if (state.tick && header->tick < *state.tick) {
        return;
    }
    const std::uint64_t part = std::uint64_t{1} << header->partIndex;
    if (!state.tick || header->tick > *state.tick) {
        _snapshotInterval =
            state.tick ? static_cast<float>(header->tick - *state.tick) * engine.fixedDeltaTime() : 0.0F;
        _partsReceived = 0;
        _snapshotEntities.clear();
    } else if ((_partsReceived & part) != 0) {
        return;
    }

    SnapshotApplier applier(registry, registry.getResource<NetworkEntityMap>(), _snapshotInterval);
    _codec->applySnapshot(applier);
    state.tick = header->tick;
    state.lastProcessedInput = header->lastProcessedInput;
    _partsReceived |= part;
    _snapshotEntities.insert(applier.seen().begin(), applier.seen().end());

    for (const auto& [entity, id, kind] : applier.spawned()) {
        engine.eventBus().publish(ReplicatedEntitySpawned{.entity = entity, .id = id, .kind = kind});
    }
    const std::uint64_t allParts =
        header->partCount == MAX_SNAPSHOT_PARTS ? ~std::uint64_t{0} : (std::uint64_t{1} << header->partCount) - 1;
    if (_partsReceived == allParts) {
        despawnMissing(registry, engine.eventBus(), _snapshotEntities);
    }
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
