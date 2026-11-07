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

namespace {

/** @brief Low 4 bits of an event counter: the wire keeps only those. */
std::uint8_t cueCount(const int count) {
    return static_cast<std::uint8_t>(static_cast<unsigned>(count) & protocol::SOUND_CUE_MAX);
}

/** @brief Strength level (0 to `SOUND_CUE_MAX`) of a speed, `SOUND_CUE_FULL_SPEED` and faster being the strongest. */
std::uint8_t cueLevel(const float speed) {
    if (!std::isfinite(speed) || speed <= 0.0F) {
        return 0;
    }
    const float level = std::min(speed / SOUND_CUE_FULL_SPEED, 1.0F) * static_cast<float>(protocol::SOUND_CUE_MAX);
    return static_cast<std::uint8_t>(std::lround(level));
}

/** @brief Speed of a strength level, the inverse of `cueLevel` (rounded to the level). */
float cueSpeed(const std::uint8_t level) {
    return static_cast<float>(level) * SOUND_CUE_FULL_SPEED / static_cast<float>(protocol::SOUND_CUE_MAX);
}

/** @brief The sound cues of an entity: a car's jumps and impacts, the ball's hits and bounces. */
std::pair<protocol::SoundCue, protocol::SoundCue> soundCues(const ecs::Registry& registry, const ecs::Entity entity) {
    const auto* cars = registry.findComponents<CarCues>();
    if (cars != nullptr && cars->contains(entity)) {
        const CarCues& cues = cars->get(entity);
        const auto kind = static_cast<std::uint8_t>(std::clamp(cues.jumpKind, 0, int{protocol::SOUND_CUE_MAX}));
        return {{.count = cueCount(cues.jumps), .value = kind},
                {.count = cueCount(cues.impacts), .value = cueLevel(cues.impactSpeed)}};
    }
    const auto* balls = registry.findComponents<BallCues>();
    if (balls != nullptr && balls->contains(entity)) {
        const BallCues& cues = balls->get(entity);
        return {{.count = cueCount(cues.hits), .value = cueLevel(cues.hitSpeed)},
                {.count = cueCount(cues.bounces), .value = cueLevel(cues.bounceSpeed)}};
    }
    return {};
}

}  // namespace

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
    // The color byte is the car's palette index, or the ball's shot color.
    const auto* looks = registry.findComponents<CarLook>();
    const auto* glows = registry.findComponents<ShotGlow>();
    std::uint8_t color = looks != nullptr && looks->contains(entity) ? looks->get(entity).color : 0;
    if (glows != nullptr && glows->contains(entity)) {
        color = static_cast<std::uint8_t>(std::clamp(glows->get(entity).shot, 0, static_cast<int>(Shot::Gold)));
    }
    const auto* statuses = registry.findComponents<CarStatus>();
    const CarStatus status = statuses != nullptr && statuses->contains(entity) ? statuses->get(entity) : CarStatus{};
    const auto [firstCue, secondCue] = soundCues(registry, entity);
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
        .flipReset = status.flipReset,
        .firstCue = firstCue,
        .secondCue = secondCue,
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
            .matchId = _matchId,
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
        .axes = {{.action = STEER_ID, .value = input->steer}, {.action = PITCH_ID, .value = input->pitch}},
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
    protocol::PlayerInput message{
        .sequence = input.sequence,
        .steer = 0.0F,
        .pitch = 0.0F,
        .jump = false,
        .boost = false,
    };
    for (const ecs::NetAxis& axis : input.axes) {
        if (axis.action == STEER_ID) {
            message.steer = axis.value;
        } else if (axis.action == PITCH_ID) {
            message.pitch = axis.value;
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
            registry.addComponent(*entity, CarStatus{
                                               .boost = static_cast<float>(state.boost),
                                               .boosting = state.boosting,
                                               .placeAtKickoff = false,
                                               .flipReset = state.flipReset,
                                           });
            registry.addComponent(*entity, CarCues{
                                               .jumps = state.firstCue.count,
                                               .jumpKind = state.firstCue.value,
                                               .impacts = state.secondCue.count,
                                               .impactSpeed = cueSpeed(state.secondCue.value),
                                           });
        } else if (state.kind == KIND_BALL) {
            registry.addComponent(*entity, ShotGlow{.shot = std::min<int>(state.color, static_cast<int>(Shot::Gold))});
            registry.addComponent(*entity, BallCues{
                                               .hits = state.firstCue.count,
                                               .hitSpeed = cueSpeed(state.firstCue.value),
                                               .bounces = state.secondCue.count,
                                               .bounceSpeed = cueSpeed(state.secondCue.value),
                                           });
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
