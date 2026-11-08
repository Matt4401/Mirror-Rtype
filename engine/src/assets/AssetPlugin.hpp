/**
 * @file AssetPlugin.hpp
 * @brief Engine plugin owning the `AssetManager` registry resource.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"

namespace assets {

/**
 * @brief Adds an `AssetManager` to the registry while the engine runs.
 *
 * The loaders are plugged in by the bridges `assets::RenderAssetsPlugin` (`engine_assets_render`) and
 * `assets::AudioAssetsPlugin` (`engine_assets_audio`), which depend on this plugin and on the backend's: they start
 * after both and detach their loaders (releasing their assets) when they shut down, before either of them.
 * Headless: depends only on `engine_core`.
 */
class AssetPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "AssetPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AssetPlugin"; }

    /**
     * @brief Adds the `AssetManager` resource.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Releases what is still loaded and removes the resource.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;
};

}  // namespace assets
