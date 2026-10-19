#include "prefabs/PrefabManager.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace prefabs {

void PrefabManager::registerPrefab(std::string name, Factory factory) {
    _factories.insert_or_assign(std::move(name), std::move(factory));
}

bool PrefabManager::has(std::string_view name) const { return _factories.contains(name); }

bool PrefabManager::spawn(std::string_view name, ecs::CommandBuffer& buffer, float x, float y) const {
    auto const it = _factories.find(name);
    if (it == _factories.end()) {
        std::cerr << "[PrefabManager] Unknown prefab: " << name << "\n";
        return false;
    }
    buffer.deferSpawn(
        [factory = it->second, x, y](ecs::Entity e, ecs::Registry& registry) { factory(e, registry, x, y); });
    return true;
}

}  // namespace prefabs
