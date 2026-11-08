/**
 * @file AssetScriptingPlugin.hpp
 * @brief Bridge between the asset manager and Wren: the `core/Assets` module and the game's declaration script.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace systems {
class ScriptSystem;
}

namespace assets {

/**
 * @brief Lets Wren scripts declare, load and release assets.
 *
 * - Adds the built-in module `core/Assets` (`Assets.texture(alias, path, group)`, `sound`, `font`, `music`,
 *   `declare(kind, ...)`, `preload*`, `loadGroup`, `loadGroupStep`, `unloadGroup`, `textureId`...), kept across hot
 *   reloads.
 * - Runs the game's declaration module, given to the constructor, at init: game plugins that require this one find
 *   its aliases declared. On a hot reload the modules run again: declaring the same assets again loads nothing, and
 *   a declaration the scripts no longer make is removed (its asset is released if nothing else needs it).
 *
 * Script arguments are untrusted: a value of the wrong type aborts the calling fiber with an error; an invalid path
 * or name, a path leaving the working directory (absolute, or starting with `..`) and an unload of the `"global"`
 * group are rejected with a warning of the manager. This module is the only one that depends on both
 * `engine_assets` and `engine_scripting`.
 */
class AssetScriptingPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the bridge.
     * @param declarationModule Wren module declaring the game's assets (`"scripts/rtype/Assets"`), or empty.
     */
    explicit AssetScriptingPlugin(std::string declarationModule = {});

    /**
     * @brief Gets plugin name.
     * @return String view "AssetScriptingPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AssetScriptingPlugin"; }

    /**
     * @brief Declares the dependencies on `AssetPlugin` and `ScriptPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Adds `core/Assets`, runs the declaration module and follows the hot reloads.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Stops following the hot reloads.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Tells whether the declaration module ran without error at init.
     * @return True if it ran, or if there is none.
     */
    [[nodiscard]] bool declarationsLoaded() const noexcept { return _declarationsLoaded; }

  private:
    /** @brief Wren module declaring the game's assets, or empty. */
    std::string _declarationModule;
    /** @brief Whether the declaration module ran at init. */
    bool _declarationsLoaded = false;
    /** @brief Script system followed for hot reloads. */
    systems::ScriptSystem* _scripts = nullptr;
    /** @brief Id of the VM reset listener. */
    std::optional<std::size_t> _resetListener;
};

}  // namespace assets
