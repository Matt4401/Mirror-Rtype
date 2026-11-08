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
    _prefabs.insert_or_assign(std::string{name},
                              Prefab{.factory = std::move(factory), .defaults = std::move(defaults)});
}

bool PrefabManager::has(std::string_view name) const { return _prefabs.contains(name); }

bool PrefabManager::spawn(std::string_view name, ecs::CommandBuffer& buffer, PrefabParams const& params) const {
    return spawn(name, buffer, params, AfterSpawn{});
}

bool PrefabManager::spawn(std::string_view name, ecs::CommandBuffer& buffer, PrefabParams const& params,
                          AfterSpawn after) const {
    auto const it = _prefabs.find(name);
    if (it == _prefabs.end()) {
        std::cerr << "[PrefabManager] Unknown prefab: " << name << "\n";
        return false;
    }
    PrefabParams merged = it->second.defaults;
    merged.merge(params);
    buffer.deferSpawn([factory = it->second.factory, merged = std::move(merged), after = std::move(after)](
                          ecs::Entity e, ecs::Registry& registry) mutable {
        factory(e, registry, merged);
        if (after) {
            after(e, registry);
        }
    });
    return true;
}

}  // namespace prefabs
