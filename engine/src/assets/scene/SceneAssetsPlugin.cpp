#include "assets/scene/SceneAssetsPlugin.hpp"

#include <stdexcept>
#include <string_view>

#include "assets/AssetManager.hpp"
#include "assets/AssetPath.hpp"
#include "assets/AssetPlugin.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "scene/SceneEvents.hpp"
#include "scene/SceneManager.hpp"
#include "scene/ScenePlugin.hpp"
#include "scene/SceneTypes.hpp"

namespace assets {

namespace {

/**
 * @brief Gets the asset group of a scene: its name, unless that cannot be a group or is the global one.
 * @param scenes Scene manager.
 * @param id Scene.
 * @return The group, or empty.
 */
std::string_view groupOf(const scene::SceneManager& scenes, const scene::SceneId id) {
    try {
        const std::string_view name = scenes.scene(id).name();
        return isValidAssetName(name) && name != AssetManager::GLOBAL_GROUP ? name : std::string_view{};
    } catch (const std::out_of_range&) {
        return {};
    }
}

}  // namespace

void SceneAssetsPlugin::build(engine::PluginBuilder& builder) {
    builder.require<scene::ScenePlugin>();
    builder.require<AssetPlugin>();
}

void SceneAssetsPlugin::init(engine::Engine& engine) {
    const scene::SceneManager* scenes = &engine.getPlugin<scene::ScenePlugin>().manager();
    ecs::Registry* registry = &engine.registry();
    const auto withGroup = [scenes, registry](const scene::SceneId id, auto&& action) {
        const std::string_view group = groupOf(*scenes, id);
        if (!group.empty() && registry->hasResource<AssetManager>()) {
            action(registry->getResource<AssetManager>(), group);
        }
    };
    ecs::EventBus& bus = engine.eventBus();
    _entering = bus.subscribe<scene::SceneEntering>([withGroup](const scene::SceneEntering& event) {
        withGroup(event.id, [](AssetManager& manager, const std::string_view group) { manager.loadGroup(group); });
    });
    _aborted = bus.subscribe<scene::SceneEnterAborted>([withGroup](const scene::SceneEnterAborted& event) {
        withGroup(event.id, [](AssetManager& manager, const std::string_view group) { manager.unloadGroup(group); });
    });
    _exited = bus.subscribe<scene::SceneExited>([withGroup](const scene::SceneExited& event) {
        withGroup(event.id, [](AssetManager& manager, const std::string_view group) { manager.unloadGroup(group); });
    });
}

void SceneAssetsPlugin::shutdown(engine::Engine& engine) {
    ecs::EventBus& bus = engine.eventBus();
    if (_entering) {
        bus.unsubscribe<scene::SceneEntering>(*_entering);
    }
    if (_aborted) {
        bus.unsubscribe<scene::SceneEnterAborted>(*_aborted);
    }
    if (_exited) {
        bus.unsubscribe<scene::SceneExited>(*_exited);
    }
    _entering.reset();
    _aborted.reset();
    _exited.reset();
}

}  // namespace assets
