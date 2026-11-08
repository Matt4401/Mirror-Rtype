/**
 * @file SceneScriptingPlugin.hpp
 * @brief Bridge between the scene module and Wren: the `core/Scene` module, scripted scenes, ownership of script
 * spawns.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "scene/SceneTypes.hpp"
#include "scene/scripting/ScriptScene.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace engine {
class Engine;
}

namespace ecs {
class Registry;
}

namespace scene {

class SceneManager;

/**
 * @brief Registry resource giving the `core/Scene` foreign methods the scene manager.
 */
struct SceneScriptBridge {
    /** @brief Manager of the engine; valid between the plugin's `init` and `shutdown`. */
    SceneManager* manager = nullptr;
};

/**
 * @brief Lets Wren scripts use scenes, and lets scenes be written in Wren.
 *
 * - Adds the built-in module `core/Scene` (`Scene.switchTo(name)`, `push`, `pop`, `replace`, `current`,
 *   `isActive`, `isRunning`), kept across hot reloads.
 * - `registerScriptScene` declares a scene driven by a Wren class (see `ScriptScene`).
 * - Entities spawned from Wren with `Engine.spawnPrefab` belong to the scene of the code that spawned them: a scripted
 *   scene, or the scene of the scripted entity; otherwise the top exclusive scene (`SceneManager::spawnTarget`).
 *
 * This module is the only one that depends on both `engine_scene` and `engine_scripting`: neither depends on the
 * other, and a game without scripting does not link it.
 */
class SceneScriptingPlugin final : public engine::IPlugin {
  public:
    /** @brief Constructs the plugin. */
    SceneScriptingPlugin() = default;

    /** @brief Destructor. */
    ~SceneScriptingPlugin() override = default;

    /** @brief Deleted copy constructor. */
    SceneScriptingPlugin(const SceneScriptingPlugin&) = delete;

    /** @brief Deleted copy assignment. */
    SceneScriptingPlugin& operator=(const SceneScriptingPlugin&) = delete;

    /** @brief Deleted move constructor. */
    SceneScriptingPlugin(SceneScriptingPlugin&&) = delete;

    /** @brief Deleted move assignment. */
    SceneScriptingPlugin& operator=(SceneScriptingPlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "SceneScriptingPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "SceneScriptingPlugin"; }

    /**
     * @brief Declares the scene and script plugins as dependencies.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Adds `core/Scene` and its bindings, the spawn ownership and the hot-reload hooks.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Removes the hooks installed by `init`.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Declares a scene driven by a Wren class.
     *
     * Can be called before `Engine::init`, once the script and scene plugins are added.
     * @param engine Engine holding the `ScenePlugin` and the `ScriptPlugin`.
     * @param name Name of the scene, used by `Scene.switchTo(name)`.
     * @param module Wren module holding the class (e.g. `scripts/scenes/Title`).
     * @param className Name of the class.
     * @param options Mode and per-frame hooks.
     * @return Id of the scene.
     */
    SceneId registerScriptScene(engine::Engine& engine, std::string name, std::string module, std::string className,
                                ScriptSceneOptions options = {});

  private:
    /**
     * @brief Spawn decorator: gives an entity spawned from Wren to the scene of its owner.
     * @param context The plugin.
     * @param registry Registry of the entity.
     * @param spawned The new entity.
     * @param owner Script owner that requested the spawn.
     */
    static void adoptSpawn(void* context, ecs::Registry& registry, ecs::Entity spawned, systems::ScriptOwner owner);

    /** @brief Scene manager, between `init` and `shutdown`. */
    SceneManager* _manager = nullptr;
    /** @brief Script system, between `init` and `shutdown`. */
    systems::ScriptSystem* _scripts = nullptr;
    /** @brief Scripted scenes, owned by the manager. */
    std::vector<ScriptScene*> _scriptScenes;
    /** @brief Id of the hot-reload listener. */
    std::optional<std::size_t> _resetListener;
};

}  // namespace scene
