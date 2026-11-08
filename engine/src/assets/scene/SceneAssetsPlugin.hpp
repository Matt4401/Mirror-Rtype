/**
 * @file SceneAssetsPlugin.hpp
 * @brief Bridge tying the asset groups to the scenes: a scene's group is loaded while the scene is on the stack.
 */

#pragma once

#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"

namespace assets {

/**
 * @brief Loads the asset group named like a scene when the scene enters, and releases it when the scene exits.
 *
 * Declare the assets of a scene in a group of its name (`Assets.texture("boss", "sprites/boss.png", "Level1")`): the
 * group is loaded on `scene::SceneEntering`, before the scene's `onEnter`, so the scene finds its handles, and
 * unloaded on `scene::SceneExited` (or `scene::SceneEnterAborted`). An asset also in `"global"` or in a scene still
 * on the stack stays loaded. A scene named `"global"` changes nothing: that group lives for the whole run. A scene
 * whose group declares nothing costs nothing.
 *
 * Headless: this module (`engine_assets_scene`) depends on `engine_assets` and `engine_scene` only.
 */
class SceneAssetsPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "SceneAssetsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "SceneAssetsPlugin"; }

    /**
     * @brief Declares the dependencies on `scene::ScenePlugin` and `AssetPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the scene events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the scene events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief `SceneEntering` subscription. */
    std::optional<ecs::EventBus::EventToken> _entering;
    /** @brief `SceneEnterAborted` subscription. */
    std::optional<ecs::EventBus::EventToken> _aborted;
    /** @brief `SceneExited` subscription. */
    std::optional<ecs::EventBus::EventToken> _exited;
};

}  // namespace assets
