#include "assets/AssetPlugin.hpp"

#include "assets/AssetManager.hpp"
#include "core/Engine.hpp"
#include "ecs/Registry.hpp"

namespace assets {

void AssetPlugin::init(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    if (!registry.hasResource<AssetManager>()) {
        registry.emplaceResource<AssetManager>(_root);
    }
}

void AssetPlugin::shutdown(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    if (registry.hasResource<AssetManager>()) {
        registry.getResource<AssetManager>().unloadAll();
        registry.removeResource<AssetManager>();
    }
}

}  // namespace assets
