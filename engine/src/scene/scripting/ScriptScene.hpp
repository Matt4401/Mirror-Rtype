/**
 * @file ScriptScene.hpp
 * @brief Scene whose behaviour is a Wren class.
 */

#pragma once

#include <string>
#include <string_view>

#include "scene/IScene.hpp"
#include "scene/SceneTypes.hpp"
#include "scripting/detail/WrenTypes.hpp"

namespace systems {
class ScriptSystem;
}

namespace scene {

/** @brief How a scripted scene is declared. */
struct ScriptSceneOptions {
    /** @brief How the scene treats the scenes below it. */
    SceneMode mode = SceneMode::Exclusive;
    /** @brief For an overlay, whether the scenes below stop running. */
    bool pausesBelow = false;
    /** @brief Whether the class has `update(dt)`, called every frame while the scene runs. */
    bool hasUpdate = false;
    /** @brief Whether the class has `fixedUpdate(dt)`, called every fixed step while the scene runs. */
    bool hasFixedUpdate = false;
};

/**
 * @brief Scene forwarding its hooks to an instance of a Wren class.
 *
 * On enter, the module is imported and `construct new()` builds the instance; on exit, the instance is released.
 * `onEnter()`, `onExit()`, `onPause()` and `onResume()` are optional and run in a fiber, so they may call
 * `Engine.wait`. `update(dt)` and `fixedUpdate(dt)` are called directly through cached handles when declared in the
 * options (no per-frame lookup, no fiber).
 *
 * While its Wren code runs, the scene is the script owner: fibers it puts to sleep are cancelled when it exits, and
 * entities it spawns with `Engine.spawnPrefab` belong to it. A hot reload rebuilds the instance without calling
 * `onEnter` again.
 */
class ScriptScene final : public IScene {
  public:
    /**
     * @brief Declares a scripted scene.
     * @param name Name of the scene, used by `Scene.switchTo(name)`.
     * @param module Wren module holding the class (e.g. `scripts/scenes/Title`).
     * @param className Name of the class.
     * @param options Mode and per-frame hooks.
     * @param scripts Script system running the class; must outlive the scene's time on the stack.
     */
    ScriptScene(std::string name, std::string module, std::string className, ScriptSceneOptions options,
                systems::ScriptSystem& scripts);

    /** @brief Destructor. */
    ~ScriptScene() override = default;

    /** @brief Deleted copy constructor. */
    ScriptScene(const ScriptScene&) = delete;

    /** @brief Deleted copy assignment. */
    ScriptScene& operator=(const ScriptScene&) = delete;

    /** @brief Deleted move constructor. */
    ScriptScene(ScriptScene&&) = delete;

    /** @brief Deleted move assignment. */
    ScriptScene& operator=(ScriptScene&&) = delete;

    /**
     * @brief Gets the name of the scene.
     * @return Name given at construction.
     */
    [[nodiscard]] std::string_view name() const noexcept override { return _name; }

    /**
     * @brief Gets the mode of the scene.
     * @return Mode given in the options.
     */
    [[nodiscard]] SceneMode mode() const noexcept override { return _options.mode; }

    /**
     * @brief Tells whether the scene pauses the scenes below.
     * @return Value given in the options.
     */
    [[nodiscard]] bool pausesBelow() const noexcept override { return _options.pausesBelow; }

    /**
     * @brief Builds the Wren instance, adds the per-frame systems, then calls `onEnter()`.
     * @param context Context of the scene.
     */
    void onEnter(SceneContext& context) override;

    /**
     * @brief Calls `onExit()`, cancels the scene's fibers and releases the instance.
     * @param context Context of the scene.
     */
    void onExit(SceneContext& context) override;

    /**
     * @brief Calls `onPause()`.
     * @param context Context of the scene.
     */
    void onPause(SceneContext& context) override;

    /**
     * @brief Calls `onResume()`.
     * @param context Context of the scene.
     */
    void onResume(SceneContext& context) override;

    /** @brief Releases every handle into the VM; called before a hot reload destroys it. */
    void releaseHandles();

    /** @brief Builds the instance again in the new VM after a hot reload, if the scene is on the stack. */
    void rebuildAfterReload();

    /**
     * @brief Tells whether the Wren instance exists.
     * @return `true` while the scene is on the stack and its class could be built.
     */
    [[nodiscard]] bool hasInstance() const noexcept { return static_cast<bool>(_instance); }

  private:
    /**
     * @brief Imports the module and builds the instance and the call handles.
     * @return `true` on success.
     */
    bool build();

    /**
     * @brief Calls a hook of `SceneHooks_` on the instance, with the scene as script owner.
     * @param hook Static method of `SceneHooks_` taking the instance (e.g. `enter(_)`).
     */
    void callHook(const scripting::detail::UniqueWrenHandle& hook);

    /**
     * @brief Calls `update(_)` or `fixedUpdate(_)` on the instance, with the scene as script owner.
     * @param method Cached call handle.
     * @param dt Time step in seconds.
     */
    void callStep(const scripting::detail::UniqueWrenHandle& method, float dt);

    /** @brief Name of the scene. */
    std::string _name;
    /** @brief Wren module of the class. */
    std::string _module;
    /** @brief Name of the class. */
    std::string _className;
    /** @brief Mode and per-frame hooks. */
    ScriptSceneOptions _options;
    /** @brief Script system running the class. */
    systems::ScriptSystem* _scripts;
    /** @brief Id of the scene, known from the first enter. */
    SceneId _id = INVALID_SCENE;
    /** @brief `true` between enter and exit. */
    bool _active = false;
    /** @brief Wren instance of the class. */
    scripting::detail::UniqueWrenHandle _instance;
    /** @brief `SceneHooks_.enter(_)` and the other hook handles. */
    scripting::detail::UniqueWrenHandle _enterHook;
    /** @brief Handle of `SceneHooks_.exit(_)`. */
    scripting::detail::UniqueWrenHandle _exitHook;
    /** @brief Handle of `SceneHooks_.pause(_)`. */
    scripting::detail::UniqueWrenHandle _pauseHook;
    /** @brief Handle of `SceneHooks_.resume(_)`. */
    scripting::detail::UniqueWrenHandle _resumeHook;
    /** @brief Handle of `update(_)`. */
    scripting::detail::UniqueWrenHandle _updateMethod;
    /** @brief Handle of `fixedUpdate(_)`. */
    scripting::detail::UniqueWrenHandle _fixedUpdateMethod;
};

}  // namespace scene
