#include "game/ReplicationCodec.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "components/InputBuffer.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "log/Logger.hpp"
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
static_assert(protocol::MAX_SNAPSHOT_PARTS <= ecs::net::MAX_SNAPSHOT_PARTS);

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
        engine::log::error(LOG_TAG, "entity ", id, " has a non-finite position: not replicated");
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

std::vector<ecs::net::Bytes> ReplicationCodec::encodeSnapshot(const std::uint32_t lastProcessedInput) {
    const std::size_t entities = _outgoing.entities.size();
    // At least one part: an empty snapshot tells the client that its entities are gone.
    const std::size_t partCount =
        std::max<std::size_t>((entities + protocol::MAX_ENTITIES_PER_PART - 1) / protocol::MAX_ENTITIES_PER_PART, 1);
    if (partCount > protocol::MAX_SNAPSHOT_PARTS) {
        engine::log::error(LOG_TAG, "snapshot of ", entities, " entities does not fit in ",
                           protocol::MAX_SNAPSHOT_PARTS, " parts: not sent");
        return {};
    }

    const auto at = [this, entities](const std::size_t part) {
        const std::size_t index = std::min(part * protocol::MAX_ENTITIES_PER_PART, entities);
        return _outgoing.entities.begin() + static_cast<std::ptrdiff_t>(index);
    };
    std::vector<ecs::net::Bytes> parts;
    parts.reserve(partCount);
    protocol::WorldState part{.tick = _outgoing.tick,
                              .lastProcessedInput = lastProcessedInput,
                              .entities = {},
                              .partIndex = 0,
                              .partCount = static_cast<std::uint32_t>(partCount)};
    for (std::size_t index = 0; index < partCount; ++index) {
        part.entities.assign(at(index), at(index + 1));
        part.partIndex = static_cast<std::uint32_t>(index);
        parts.push_back(protocol::encode(part));
    }
    return parts;
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
    return ecs::net::SnapshotHeader{.tick = _incoming.tick,
                                    .lastProcessedInput = _incoming.lastProcessedInput,
                                    .partIndex = _incoming.partIndex,
                                    .partCount = _incoming.partCount};
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
