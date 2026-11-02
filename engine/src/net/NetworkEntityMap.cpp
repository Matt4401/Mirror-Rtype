#include "net/NetworkEntityMap.hpp"

#include <cstdint>
#include <optional>

#include "ecs/Entity.hpp"

namespace ecs::net {

std::uint32_t NetworkEntityMap::assign(const ecs::Entity entity) {
    if (const auto existing = idOf(entity)) {
        return *existing;
    }
    const std::uint32_t id = _nextId++;
    _entities.emplace(id, entity);
    _ids.emplace(entity, id);
    return id;
}

bool NetworkEntityMap::bind(const std::uint32_t id, const ecs::Entity entity) {
    if (id == 0) {
        return false;
    }
    unbind(id);
    unbindEntity(entity);
    _entities.emplace(id, entity);
    _ids.emplace(entity, id);
    return true;
}

void NetworkEntityMap::unbind(const std::uint32_t id) {
    const auto it = _entities.find(id);
    if (it == _entities.end()) {
        return;
    }
    _ids.erase(it->second);
    _entities.erase(it);
}

void NetworkEntityMap::unbindEntity(const ecs::Entity entity) {
    const auto it = _ids.find(entity);
    if (it == _ids.end()) {
        return;
    }
    _entities.erase(it->second);
    _ids.erase(it);
}

std::optional<ecs::Entity> NetworkEntityMap::entityOf(const std::uint32_t id) const {
    const auto it = _entities.find(id);
    return it == _entities.end() ? std::nullopt : std::optional{it->second};
}

std::optional<std::uint32_t> NetworkEntityMap::idOf(const ecs::Entity entity) const {
    const auto it = _ids.find(entity);
    return it == _ids.end() ? std::nullopt : std::optional{it->second};
}

}  // namespace ecs::net
