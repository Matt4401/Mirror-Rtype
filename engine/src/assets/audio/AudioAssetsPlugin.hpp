/**
 * @file AudioAssetsPlugin.hpp
 * @brief Bridge plugging the audio module's sound and music loaders into the asset manager.
 */

#pragma once

#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace assets {

/**
 * @brief Loads the `assets::Sound` and `assets::Music` kinds through the backend of `audio::AudioPlugin`.
 *
 * Starts after `audio::AudioPlugin` and `AssetPlugin`, and stops before both, so the sounds are released while the
 * audio device is still open. With a muted backend (no audio device, `NullAudioBackend`) no loader is installed: the
 * sounds stay declared and `AssetStatus::Declared` instead of failing one by one with a warning each.
 *
 * This module (`engine_assets_audio`) is the only one that depends on both `engine_assets` and `engine_audio`.
 */
class AudioAssetsPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "AudioAssetsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AudioAssetsPlugin"; }

    /**
     * @brief Declares the dependencies on `audio::AudioPlugin` and `AssetPlugin`.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Installs the sound and music loaders, unless the backend is muted.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Releases the sounds and musics of the manager and removes the loaders.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Tells whether the loaders are installed.
     * @return False before init, after shutdown, or with a muted backend.
     */
    [[nodiscard]] bool attached() const noexcept { return _attached; }

  private:
    /** @brief Whether the loaders are installed. */
    bool _attached = false;
};

}  // namespace assets
