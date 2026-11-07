#include "net/SnapshotApplier.hpp"

#include <cstdint>
#include <optional>

#include "components/Interpolated.hpp"
#include "components/NetworkId.hpp"
#include "components/Predicted.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "net/NetworkEntityMap.hpp"

namespace ecs::net {

SnapshotApplier::SnapshotApplier(ecs::Registry& registry, NetworkEntityMap& map, const float interval)
    : _registry(&registry), _map(&map), _interval(interval) {}

std::optional<ecs::Entity> SnapshotApplier::entity(const std::uint32_t id, const std::uint32_t kind) {
    if (id == 0) {
        return std::nullopt;
    }
    if (const auto existing = _map->entityOf(id); existing && _registry->isAlive(*existing)) {
        _seen.insert(*existing);
        return existing;
    }
    const ecs::Entity created = _registry->spawnEntity();
    _map->bind(id, created);
    _registry->emplaceComponent<ecs::NetworkId>(created, id);
    _registry->emplaceComponent<ecs::Replicated>(created, kind);
    _seen.insert(created);
    _spawned.push_back({.entity = created, .id = id, .kind = kind});
    return created;
}

void SnapshotApplier::setTransform(const ecs::Entity entity, const ecs::Transform& transform) {
    if (!_registry->isAlive(entity)) {
        return;
    }
    // A predicted entity is reset to the server state at once, then reconciled: interpolating it would lag behind.
    if (_registry->hasComponent<ecs::Interpolated>(entity) && !_registry->hasComponent<ecs::Predicted>(entity)) {
        auto& interp = _registry->getComponents<ecs::Interpolated>().get(entity);
        const ecs::Transform current = _registry->hasComponent<ecs::Transform>(entity)
                                           ? _registry->getComponents<ecs::Transform>().get(entity)
                                           : transform;
        interp.from = interp.initialized ? current : transform;
        interp.to = transform;
        interp.elapsed = 0.0F;
        interp.duration = interp.initialized ? _interval : 0.0F;
        interp.initialized = true;
        if (!_registry->hasComponent<ecs::Transform>(entity)) {
            _registry->addComponent(entity, transform);
        }
        return;
    }
    if (_registry->hasComponent<ecs::Transform>(entity)) {
        _registry->getComponents<ecs::Transform>().get(entity) = transform;
    } else {
        _registry->addComponent(entity, transform);
    }
}

}  // namespace ecs::net
