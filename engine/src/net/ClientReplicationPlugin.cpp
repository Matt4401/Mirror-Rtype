#include "net/ClientReplicationPlugin.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
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

namespace {

/** @brief Inputs kept to measure their round trip: about 4 seconds of inputs at 60 Hz, more than any playable lag. */
constexpr std::size_t MAX_PENDING_INPUTS = 256;
/** @brief Weight of a new sample in the smoothed round trip. */
constexpr float RTT_SMOOTHING = 0.1F;
constexpr double MILLISECONDS_PER_SECOND = 1000.0;

}  // namespace

ClientReplicationPlugin::ClientReplicationPlugin(std::shared_ptr<IReplicationCodec> codec) : _codec(std::move(codec)) {}

void ClientReplicationPlugin::build(engine::PluginBuilder& builder) { builder.require<NetworkPlugin>(); }

void ClientReplicationPlugin::init(engine::Engine& engine) {
    auto& registry = engine.registry();
    if (!registry.hasResource<SnapshotState>()) {
        registry.emplaceResource<SnapshotState>();
    }
    if (!registry.hasResource<ReplicationStats>()) {
        registry.emplaceResource<ReplicationStats>();
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

    _pendingInputs.push_back({.sequence = local.input.sequence, .sentAt = engine.clock().now()});
    if (_pendingInputs.size() > MAX_PENDING_INPUTS) {
        _pendingInputs.pop_front();
    }
    _sentInputs.push_back({.sequence = local.input.sequence, .bytes = std::move(bytes)});
    if (_sentInputs.size() > INPUT_RESEND_COUNT) {
        _sentInputs.pop_front();
    }
    predict(engine.registry(), engine.eventBus(), local.input, engine.fixedDeltaTime());
}

void ClientReplicationPlugin::measureInputRtt(engine::Engine& engine, const std::uint32_t acknowledged) {
    std::optional<double> sentAt;
    while (!_pendingInputs.empty() && _pendingInputs.front().sequence <= acknowledged) {
        if (_pendingInputs.front().sequence == acknowledged) {
            sentAt = _pendingInputs.front().sentAt;
        }
        _pendingInputs.pop_front();
    }
    if (!sentAt) {
        return;
    }
    auto& stats = engine.registry().getResource<ReplicationStats>();
    const auto sample = static_cast<float>((engine.clock().now() - *sentAt) * MILLISECONDS_PER_SECOND);
    stats.inputRttMs = stats.inputRttMs ? *stats.inputRttMs + (RTT_SMOOTHING * (sample - *stats.inputRttMs)) : sample;
}

void ClientReplicationPlugin::onPacket(engine::Engine& engine, const PacketReceived& packet) {
    const auto header = _codec->decodeSnapshot(packet.data);
    if (!header || header->partCount == 0 || header->partCount > MAX_SNAPSHOT_PARTS ||
        header->partIndex >= header->partCount) {
        return;
    }
    auto& registry = engine.registry();
    auto& state = registry.getResource<SnapshotState>();
    auto& stats = registry.getResource<ReplicationStats>();
    // Unreliable channel: an older snapshot can arrive after a newer one; applying it would move entities back.
    if (state.tick && header->tick < *state.tick) {
        ++stats.snapshotsStale;
        return;
    }
    const std::uint64_t part = std::uint64_t{1} << header->partIndex;
    if (!state.tick || header->tick > *state.tick) {
        _snapshotInterval =
            state.tick ? static_cast<float>(header->tick - *state.tick) * engine.fixedDeltaTime() : 0.0F;
        _partsReceived = 0;
        _snapshotEntities.clear();
        stats.snapshotsMissed += state.tick ? header->tick - *state.tick - 1 : 0;
        ++stats.snapshotsReceived;
    } else if ((_partsReceived & part) != 0) {
        return;
    }

    SnapshotApplier applier(registry, registry.getResource<NetworkEntityMap>(), _snapshotInterval);
    _codec->applySnapshot(applier);
    state.tick = header->tick;
    state.lastProcessedInput = header->lastProcessedInput;
    measureInputRtt(engine, header->lastProcessedInput);
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

void ClientReplicationPlugin::reset(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    despawnMissing(registry, engine.eventBus(), {});
    if (registry.hasResource<SnapshotState>()) {
        registry.getResource<SnapshotState>() = SnapshotState{};
    }
    _sentInputs.clear();
    _snapshotInterval = 0.0F;
    _partsReceived = 0;
    _snapshotEntities.clear();
}

}  // namespace ecs::net
