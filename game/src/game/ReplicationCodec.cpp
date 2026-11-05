#include "game/ReplicationCodec.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

#include "Logger.hpp"
#include "components/InputBuffer.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "net/INetworkManager.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/NetworkConfig.hpp"
#include "net/SnapshotApplier.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace rtype::game {

namespace {

constexpr const char* LOG_TAG = "replication";

// The engine's transport refuses larger datagrams: every snapshot that passes the size check can be sent.
static_assert(protocol::MAX_UNRELIABLE_SIZE <= ecs::net::MAX_UNRELIABLE_PAYLOAD);

}  // namespace

void ReplicationCodec::beginSnapshot(const std::uint32_t tick) {
    _outgoing.tick = tick;
    _outgoing.entities.clear();
}

void ReplicationCodec::writeEntity(const ecs::Registry& registry, const ecs::Entity entity, const std::uint32_t id,
                                   const ecs::Replicated& replicated) {
    ecs::Transform transform;
    if (registry.hasComponent<ecs::Transform>(entity)) {
        transform = registry.getComponents<ecs::Transform>().get(entity);
    }
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y)) {
        log::error(LOG_TAG, "entity ", id, " has a non-finite position: not replicated");
        return;
    }
    EntityNetState state;
    if (registry.hasComponent<EntityNetState>(entity)) {
        state = registry.getComponents<EntityNetState>().get(entity);
    }
    _outgoing.entities.push_back({
        .entityId = id,
        .kind = replicated.kind,
        .subtype = state.subtype,
        .x = transform.x,
        .y = transform.y,
        .health = state.health,
        .flags = state.flags,
    });
}

ecs::net::Bytes ReplicationCodec::encodeSnapshot(const std::uint32_t lastProcessedInput) {
    _outgoing.lastProcessedInput = lastProcessedInput;
    ecs::net::Bytes bytes = protocol::encode(_outgoing);
    if (bytes.size() > protocol::MAX_UNRELIABLE_SIZE) {
        log::error(LOG_TAG, "snapshot of ", bytes.size(), " bytes does not fit in a datagram: not sent");
        return {};
    }
    return bytes;
}

std::optional<ecs::NetInput> ReplicationCodec::decodeInput(const ecs::net::Bytes& data) {
    const auto message = protocol::decode(data);
    const auto* input = message ? std::get_if<protocol::PlayerInput>(&*message) : nullptr;
    if (input == nullptr) {
        return std::nullopt;
    }
    ecs::NetInput decoded{.sequence = input->sequence, .held = input->held, .axes = {}};
    decoded.axes.reserve(input->axes.size());
    for (const auto& [action, value] : input->axes) {
        decoded.axes.push_back({.action = action, .value = value});
    }
    return decoded;
}

ecs::net::Bytes ReplicationCodec::encodeInput(const ecs::NetInput& input) {
    protocol::PlayerInput encoded{.sequence = input.sequence, .held = input.held, .axes = {}};
    encoded.axes.reserve(input.axes.size());
    for (const auto& [action, value] : input.axes) {
        encoded.axes.push_back({.action = action, .value = value});
    }
    return protocol::encode(encoded);
}

std::optional<ecs::net::SnapshotHeader> ReplicationCodec::decodeSnapshot(const ecs::net::Bytes& data) {
    auto message = protocol::decode(data);
    auto* snapshot = message ? std::get_if<protocol::WorldState>(&*message) : nullptr;
    if (snapshot == nullptr) {
        return std::nullopt;
    }
    _incoming = std::move(*snapshot);
    return ecs::net::SnapshotHeader{.tick = _incoming.tick, .lastProcessedInput = _incoming.lastProcessedInput};
}

void ReplicationCodec::applySnapshot(ecs::net::SnapshotApplier& applier) {
    ecs::Registry& registry = applier.registry();
    for (const protocol::EntityState& state : _incoming.entities) {
        const auto entity = applier.entity(state.entityId, state.kind);
        if (!entity) {
            continue;
        }
        ecs::Transform transform;
        if (registry.hasComponent<ecs::Transform>(*entity)) {
            transform = registry.getComponents<ecs::Transform>().get(*entity);
        }
        transform.x = state.x;
        transform.y = state.y;
        applier.setTransform(*entity, transform);

        registry.addComponent(*entity,
                              EntityNetState{.subtype = state.subtype, .health = state.health, .flags = state.flags});
    }
}

}  // namespace rtype::game
