/**
 * @file ScenePlugin.hpp
 * @brief Plugin driving the `SceneManager`: transitions at the frame boundary, then the systems of running scenes.
 */

#pragma once

#include <memory>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "scene/SceneManager.hpp"

namespace scene {

/**
 * @brief Runs the scenes of a game.
 *
 * - `preUpdate`: applies the queued transitions, so every stage of the frame sees the same stack.
 * - `fixedUpdate`, `update`, `lateUpdate`: run the systems of the running scenes, bottom of the stack first.
 * - `shutdown`: exits every scene, running their `onExit` hooks.
 *
 * The manager is also registered in the Mediator (`engine.service<scene::SceneManager>()`), so any plugin can query or
 * change the stack without knowing this plugin. It has no dependency on rendering, networking or scripting: a headless
 * server can use it too.
 *
 * Scenes are usually registered before `Engine::init`, through `manager()`; a first `switchTo` queued then is applied
 * on the first frame.
 */
class ScenePlugin final : public engine::IPlugin {
  public:
    /** @brief Creates the plugin and its empty manager. */
    ScenePlugin() : _manager(std::make_shared<SceneManager>()) {}

    /** @brief Destructor. */
    ~ScenePlugin() override = default;

    /** @brief Deleted copy constructor. */
    ScenePlugin(const ScenePlugin&) = delete;

    /** @brief Deleted copy assignment. */
    ScenePlugin& operator=(const ScenePlugin&) = delete;

    /** @brief Deleted move constructor. */
    ScenePlugin(ScenePlugin&&) = delete;

    /** @brief Deleted move assignment. */
    ScenePlugin& operator=(ScenePlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "ScenePlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ScenePlugin"; }

    /**
     * @brief Declares the core plugin as a dependency and the scene manager as a provided service.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Binds the manager to the engine and registers it in the Mediator.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Applies the queued scene transitions.
     * @param engine Host engine (unused).
     */
    void preUpdate(engine::Engine& engine) override;

    /**
     * @brief Runs the fixed-step systems of the running scenes.
     * @param engine Host engine.
     * @param time Fixed time.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Runs the per-frame systems of the running scenes.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Runs the late per-frame systems of the running scenes.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void lateUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Exits every scene and removes the manager from the Mediator.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Gets the scene manager.
     * @return The manager, usable before `init` to register scenes.
     */
    [[nodiscard]] SceneManager& manager() noexcept { return *_manager; }

  private:
    /** @brief The scene manager, shared with the Mediator. */
    std::shared_ptr<SceneManager> _manager;
};

}  // namespace scene
