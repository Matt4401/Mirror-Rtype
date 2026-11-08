/**
 * @file RenderAssetsPlugin.hpp
 * @brief Bridge plugging the render module's texture and font loaders into the asset manager.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace assets {

/**
 * @brief Loads the `assets::Texture` and `assets::Font` kinds through `render::RenderPlugin`.
 *
 * Starts after `render::RenderPlugin` and `AssetPlugin`, and stops before both, so the textures and fonts are released
 * while the window still exists. A texture that cannot be loaded is served as the store's magenta checkerboard
 * (`AssetLoader::fallback`) and reported as `AssetStatus::Failed`. Handles are given back when an asset is loaded again
 * (`TextureStore::load(..., preferred)`).
 *
 * This module (`engine_assets_render`) is the only one that depends on both `engine_assets` and `engine_render`:
 * neither of them knows the other.
 */
class RenderAssetsPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "RenderAssetsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "RenderAssetsPlugin"; }

    /**
     * @brief Declares the dependencies on `render::RenderPlugin` and `AssetPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Installs the texture and font loaders.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Releases the textures and fonts of the manager and removes the loaders.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Whether the loaders are installed. */
    bool _attached = false;
};

}  // namespace assets
