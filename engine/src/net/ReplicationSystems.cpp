#include "net/ReplicationSystems.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <unordered_map>
#include <vector>

#include "components/InputBuffer.hpp"
#include "components/Interpolated.hpp"
#include "components/NetworkId.hpp"
#include "components/NetworkOwner.hpp"
#include "components/Predicted.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/InputQueue.hpp"
#include "net/NetworkConfig.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/SnapshotApplier.hpp"

namespace ecs::net {

namespace {

/** @brief An input to publish once the iteration over the components is over. */
struct PendingInput {
    ecs::Entity entity;
    NetInput input;
};

// Handlers may spawn or kill entities: events are published after the iteration, never during it, so that the dense
// arrays being walked are not modified.
void publishInputs(ecs::EventBus& events, const std::vector<PendingInput>& inputs, const float deltaTime,
                   const bool replayed) {
    for (const auto& [entity, input] : inputs) {
        events.publish(InputApplied{.entity = entity, .input = input, .deltaTime = deltaTime, .replayed = replayed});
    }
}

float lerp(const float from, const float to, const float alpha) { return from + ((to - from) * alpha); }

// Shortest way around the circle, so that going from 350 to 10 degrees turns by 20 degrees, not 340.
float lerpAngle(const float from, const float to, const float alpha) {
    const float delta = std::remainder(to - from, 2.0F * std::numbers::pi_v<float>);
    return from + (delta * alpha);
}

}  // namespace

std::uint32_t replicate(ecs::Registry& registry, const ecs::Entity entity, const std::uint32_t kind) {
    auto& map = registry.hasResource<NetworkEntityMap>() ? registry.getResource<NetworkEntityMap>()
                                                         : registry.emplaceResource<NetworkEntityMap>();
    const std::uint32_t id = map.assign(entity);
    registry.addComponent(entity, ecs::NetworkId{id});
    registry.addComponent(entity, ecs::Replicated{kind});
    return id;
}

std::optional<ecs::Entity> findOwnedEntity(ecs::Registry& registry, const PeerId peer) {
    std::optional<ecs::Entity> found;
    registry.runSystem<ecs::NetworkOwner, ecs::InputBuffer>(
        [&](const ecs::Entity entity, const ecs::NetworkOwner& owner, ecs::InputBuffer& /*buffer*/) {
            if (!found && owner.client == peer) {
                found = entity;
            }
        });
    return found;
}

bool receiveInput(ecs::Registry& registry, const PeerId peer, const NetInput& input) {
    const auto entity = findOwnedEntity(registry, peer);
    if (!entity) {
        return false;
    }
    return pushInput(registry.getComponents<ecs::InputBuffer>().get(*entity), input);
}

void applyInputs(ecs::Registry& registry, ecs::EventBus& events, const float deltaTime) {
    std::vector<PendingInput> inputs;
    // One input per step, like the client prediction: the server stays at the same pace as the client.
    registry.runSystem<ecs::InputBuffer>([&](const ecs::Entity entity, ecs::InputBuffer& buffer) {
        if (const auto input = popInput(buffer)) {
            inputs.push_back({.entity = entity, .input = *input});
        }
    });
    publishInputs(events, inputs, deltaTime, false);
}

std::size_t sendSnapshots(ecs::Registry& registry, IReplicationCodec& codec, engine::INetworkService& service,
                          const std::uint32_t tick) {
    codec.beginSnapshot(tick);
    registry.runSystem<ecs::NetworkId, ecs::Replicated>(
        [&](const ecs::Entity entity, const ecs::NetworkId& id, const ecs::Replicated& replicated) {
            codec.writeEntity(registry, entity, id.value, replicated);
        });

    // A peer may control several entities: it receives one snapshot, acknowledging its latest applied input.
    std::unordered_map<PeerId, std::uint32_t> recipients;
    registry.runSystem<ecs::NetworkOwner>([&](const ecs::Entity entity, const ecs::NetworkOwner& owner) {
        std::uint32_t& last = recipients[owner.client];
        if (registry.hasComponent<ecs::InputBuffer>(entity)) {
            last = std::max(last, registry.getComponents<ecs::InputBuffer>().get(entity).lastProcessed);
        }
    });

    std::size_t sent = 0;
    for (const auto& [peer, lastProcessed] : recipients) {
        const Bytes bytes = codec.encodeSnapshot(lastProcessed);
        if (bytes.empty() || bytes.size() > MAX_UNRELIABLE_PAYLOAD) {
            continue;
        }
        service.send(peer, bytes, Channel::Unreliable);
        ++sent;
    }
    return sent;
}

void despawnMissing(ecs::Registry& registry, ecs::EventBus& events, const SnapshotApplier& applier) {
    std::vector<ReplicatedEntityDespawned> missing;
    registry.runSystem<ecs::NetworkId>([&](const ecs::Entity entity, const ecs::NetworkId& id) {
        if (!applier.contains(entity)) {
            const std::uint32_t kind = registry.hasComponent<ecs::Replicated>(entity)
                                           ? registry.getComponents<ecs::Replicated>().get(entity).kind
                                           : 0;
            missing.push_back({.entity = entity, .id = id.value, .kind = kind});
        }
    });
    for (const auto& despawned : missing) {
        events.publish(despawned);
        if (registry.isAlive(despawned.entity)) {
            registry.killEntity(despawned.entity);
        }
    }
}

void reconcile(ecs::Registry& registry, ecs::EventBus& events, const SnapshotApplier& applier,
               const std::uint32_t lastProcessedInput, const float deltaTime) {
    std::vector<PendingInput> replays;
    registry.runSystem<ecs::Predicted>([&](const ecs::Entity entity, ecs::Predicted& predicted) {
        auto& history = predicted.history;
        while (!history.empty() && history.front().sequence <= lastProcessedInput) {
            history.pop_front();
        }
        if (!applier.contains(entity)) {
            return;
        }
        for (const NetInput& input : history) {
            replays.push_back({.entity = entity, .input = input});
        }
    });
    publishInputs(events, replays, deltaTime, true);
}

void predict(ecs::Registry& registry, ecs::EventBus& events, const NetInput& input, const float deltaTime) {
    std::vector<PendingInput> inputs;
    registry.runSystem<ecs::Predicted>([&](const ecs::Entity entity, ecs::Predicted& predicted) {
        if (predicted.capacity == 0) {
            return;
        }
        predicted.history.push_back(input);
        while (predicted.history.size() > predicted.capacity) {
            predicted.history.pop_front();
        }
        inputs.push_back({.entity = entity, .input = input});
    });
    publishInputs(events, inputs, deltaTime, false);
}

void interpolate(ecs::Registry& registry, const float deltaTime) {
    registry.runSystem<ecs::Interpolated, ecs::Transform>(
        [&registry, deltaTime](const ecs::Entity entity, ecs::Interpolated& interp, ecs::Transform& transform) {
            if (!interp.initialized || registry.hasComponent<ecs::Predicted>(entity)) {
                return;
            }
            interp.elapsed = std::min(interp.elapsed + deltaTime, interp.duration);
            const float alpha = interp.duration > 0.0F ? interp.elapsed / interp.duration : 1.0F;
            transform.x = lerp(interp.from.x, interp.to.x, alpha);
            transform.y = lerp(interp.from.y, interp.to.y, alpha);
            transform.rotation = lerpAngle(interp.from.rotation, interp.to.rotation, alpha);
            transform.z = interp.to.z;
            transform.scaleX = interp.to.scaleX;
            transform.scaleY = interp.to.scaleY;
        });
}

}  // namespace ecs::net
