#include "prefabs/PrefabManager.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include "ecs/CommandBuffer.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace prefabs {

void PrefabManager::registerPrefab(std::string_view name, Factory factory, PrefabParams defaults) {
    _prefabs.insert_or_assign(std::string{name}, Prefab{std::move(factory), std::move(defaults)});
}

bool PrefabManager::has(std::string_view name) const { return _prefabs.contains(name); }

bool PrefabManager::spawn(std::string_view name, ecs::CommandBuffer& buffer, PrefabParams const& params) const {
    auto const it = _prefabs.find(name);
    if (it == _prefabs.end()) {
        std::cerr << "[PrefabManager] Unknown prefab: " << name << "\n";
        return false;
    }
    PrefabParams merged = it->second.defaults;
    merged.merge(params);
    Factory const* factory = &it->second.factory;
    buffer.deferSpawn([factory, merged = std::move(merged)](ecs::Entity e, ecs::Registry& registry) {
        (*factory)(e, registry, merged);
    });
    return true;
}

}  // namespace prefabs
