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
#include "components/Interpolated.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "net/INetworkManager.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/SnapshotApplier.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::game {

void ReplicationCodec::beginSnapshot(const std::uint32_t tick) {
    _tick = tick;
    _entities.clear();
}

void ReplicationCodec::writeEntity(const ecs::Registry& registry, const ecs::Entity entity, const std::uint32_t id,
                                   const ecs::Replicated& replicated) {
    const auto* transforms = registry.findComponents<ecs::Transform>();
    if (transforms == nullptr || !transforms->contains(entity)) {
        return;
    }
    const ecs::Transform& transform = transforms->get(entity);
    const auto* looks = registry.findComponents<CarLook>();
    const std::uint8_t color = looks != nullptr && looks->contains(entity) ? looks->get(entity).color : 0;
    const auto* statuses = registry.findComponents<CarStatus>();
    const CarStatus status = statuses != nullptr && statuses->contains(entity) ? statuses->get(entity) : CarStatus{};
    _entities.push_back(protocol::EntityState{
        .id = id,
        .kind = static_cast<std::uint8_t>(replicated.kind),
        .x = transform.x,
        .y = transform.y,
        .rotation = transform.rotation,
        .facingLeft = transform.scaleX < 0.0F,
        .color = color,
        .boost = static_cast<std::uint8_t>(std::clamp(std::lround(status.boost), 0L, 100L)),
        .boosting = status.boosting,
    });
}

std::vector<ecs::net::Bytes> ReplicationCodec::encodeSnapshot(const std::uint32_t lastProcessedInput) {
    const std::size_t perPart = protocol::MAX_ENTITIES_PER_PART;
    const std::size_t partCount = std::max<std::size_t>(1, (_entities.size() + perPart - 1) / perPart);
    if (partCount > protocol::MAX_SNAPSHOT_PARTS) {
        return {};
    }
    std::vector<ecs::net::Bytes> parts;
    parts.reserve(partCount);
    for (std::size_t part = 0; part < partCount; ++part) {
        const auto first = static_cast<std::ptrdiff_t>(std::min(part * perPart, _entities.size()));
        const auto last = static_cast<std::ptrdiff_t>(std::min((part + 1) * perPart, _entities.size()));
        parts.push_back(protocol::encode(protocol::Snapshot{
            .tick = _tick,
            .lastProcessedInput = lastProcessedInput,
            .partIndex = static_cast<std::uint8_t>(part),
            .partCount = static_cast<std::uint8_t>(partCount),
            .entities = {_entities.begin() + first, _entities.begin() + last},
        }));
    }
    return parts;
}

std::optional<ecs::NetInput> ReplicationCodec::decodeInput(const ecs::net::Bytes& data) {
    const auto message = protocol::decode(data);
    const auto* input = message ? std::get_if<protocol::PlayerInput>(&*message) : nullptr;
    if (input == nullptr) {
        return std::nullopt;
    }
    ecs::NetInput result{
        .sequence = input->sequence,
        .held = {},
        .axes = {{.action = STEER_ID, .value = input->steer}},
    };
    if (input->jump) {
        result.held.push_back(JUMP_ID);
    }
    if (input->boost) {
        result.held.push_back(BOOST_ID);
    }
    return result;
}

ecs::net::Bytes ReplicationCodec::encodeInput(const ecs::NetInput& input) {
    protocol::PlayerInput message{.sequence = input.sequence, .steer = 0.0F, .jump = false, .boost = false};
    for (const ecs::NetAxis& axis : input.axes) {
        if (axis.action == STEER_ID) {
            message.steer = axis.value;
        }
    }
    message.jump = std::ranges::find(input.held, JUMP_ID) != input.held.end();
    message.boost = std::ranges::find(input.held, BOOST_ID) != input.held.end();
    return protocol::encode(message);
}

std::optional<ecs::net::SnapshotHeader> ReplicationCodec::decodeSnapshot(const ecs::net::Bytes& data) {
    auto message = protocol::decode(data);
    auto* snapshot = message ? std::get_if<protocol::Snapshot>(&*message) : nullptr;
    if (snapshot == nullptr) {
        return std::nullopt;
    }
    _received = std::move(snapshot->entities);
    return ecs::net::SnapshotHeader{
        .tick = snapshot->tick,
        .lastProcessedInput = snapshot->lastProcessedInput,
        .partIndex = snapshot->partIndex,
        .partCount = snapshot->partCount,
    };
}

void ReplicationCodec::applySnapshot(ecs::net::SnapshotApplier& applier) {
    ecs::Registry& registry = applier.registry();
    for (const protocol::EntityState& state : _received) {
        const auto entity = applier.entity(state.id, state.kind);
        if (!entity) {
            continue;
        }
        if (!registry.hasComponent<ecs::Interpolated>(*entity)) {
            registry.addComponent(*entity, ecs::Interpolated{});
        }
        if (state.kind == KIND_CAR) {
            registry.addComponent(*entity, CarLook{.color = state.color});
            registry.addComponent(*entity,
                                  CarStatus{.boost = static_cast<float>(state.boost), .boosting = state.boosting});
        }
        applier.setTransform(*entity, ecs::Transform{
                                          .x = state.x,
                                          .y = state.y,
                                          .z = 0.0F,
                                          .rotation = state.rotation,
                                          .scaleX = state.facingLeft ? -1.0F : 1.0F,
                                          .scaleY = 1.0F,
                                      });
    }
}

}  // namespace rlss::game
