/**
 * @file AssetPlugin.hpp
 * @brief Engine plugin owning the `AssetManager` registry resource.
 */

#pragma once

#include <filesystem>
#include <string_view>
#include <utility>

#include "core/IPlugin.hpp"

namespace assets {

/**
 * @brief Adds an `AssetManager` to the registry while the engine runs.
 *
 * The rendering and audio plugins list it as an optional dependency: when it is installed they start after it, plug
 * their loaders into the manager, and detach them (releasing their assets) when they shut down, before this plugin.
 * Headless: depends only on `engine_core`.
 */
class AssetPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Creates the plugin.
     * @param root Directory the manager loads relative paths from, usually `engine::contentRoot()`; empty for the
     * working directory.
     */
    explicit AssetPlugin(std::filesystem::path root = {}) : _root(std::move(root)) {}

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

  private:
    /** @brief Content root given to the manager. */
    std::filesystem::path _root;
};

}  // namespace assets
