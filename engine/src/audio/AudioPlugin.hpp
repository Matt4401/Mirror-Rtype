/**
 * @file AudioPlugin.hpp
 * @brief Audio lifecycle plugin owning the audio backend and the AudioSystem.
 */

#pragma once

#include <memory>
#include <string_view>

#include "audio/IAudioBackend.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "systems/audio/AudioSystem.hpp"

namespace audio {

/**
 * @brief Plugin opening the audio output, running the `AudioSystem` and exposing `engine::IAudioService`.
 *
 * Gameplay code plays sounds by publishing audio events on the engine `EventBus`; the plugin executes them once
 * per frame during the render phase, after every update, so emitters follow the final entity positions.
 * When no audio device can be opened, the plugin falls back to a muted backend and the game keeps running.
 */
class AudioPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Constructs an AudioPlugin that creates its backend from settings at init.
     * @param settings Backend kind, voice limits, volumes and spatialization, usually read with `readAudioSettings`.
     */
    explicit AudioPlugin(AudioSettings settings = AudioSettings{});

    /**
     * @brief Constructs an AudioPlugin around an already created backend (custom library, tests).
     *
     * The backend kind and voice limits of `settings` are ignored; a null `backend` is replaced by a muted one.
     * The backend lives as long as the plugin, so the engine can be shut down and initialized again.
     *
     * @param backend Audio output to use; ownership is transferred to the plugin.
     * @param settings Volumes and spatialization to apply at init.
     */
    explicit AudioPlugin(std::unique_ptr<IAudioBackend> backend, AudioSettings settings = AudioSettings{});

    /** @brief Destructor releasing the audio system before its backend. */
    ~AudioPlugin() override;

    /** @brief Deleted copy constructor. */
    AudioPlugin(const AudioPlugin&) = delete;

    /** @brief Deleted copy assignment operator. */
    AudioPlugin& operator=(const AudioPlugin&) = delete;

    /** @brief Deleted move constructor. */
    AudioPlugin(AudioPlugin&&) = delete;

    /** @brief Deleted move assignment operator. */
    AudioPlugin& operator=(AudioPlugin&&) = delete;

    /**
     * @brief Gets plugin name.
     * @return String view "AudioPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "AudioPlugin"; }

    /**
     * @brief Declares plugin dependencies (requires CorePlugin, provides IAudioService).
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Opens the backend, creates the AudioSystem and registers IAudioService.
     * @param engine Reference to host Engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Executes the audio events of the frame and refills the music stream.
     * @param engine Reference to host Engine.
     */
    void render(engine::Engine& engine) override;

    /**
     * @brief Unregisters IAudioService, stops every sound and closes the backend.
     * @param engine Reference to host Engine.
     */
    void shutdown(engine::Engine& engine) override;

    /** @brief Gets the active AudioSystem, or nullptr if uninitialized. */
    [[nodiscard]] systems::AudioSystem* audioSystem() noexcept { return _system.get(); }

    /**
     * @brief Gets the owned backend, or nullptr when none is owned.
     *
     * An injected backend is owned for the plugin lifetime, so a non-null result does not mean the plugin is
     * initialized. A settings-built backend is created by `init()` and released by `shutdown()`. If `init()` throws,
     * the backend remains owned.
     */
    [[nodiscard]] IAudioBackend* backend() noexcept { return _backend.get(); }

  private:
    class AudioServiceImpl;

    /** @brief Settings applied at init. */
    AudioSettings _settings;
    /** @brief Audio output; declared before `_system`, which must be destroyed first. */
    std::unique_ptr<IAudioBackend> _backend = nullptr;
    /** @brief Executes the audio events; references `_backend`. */
    std::unique_ptr<systems::AudioSystem> _system = nullptr;
    /** @brief Abstract audio service adapter registered in the Mediator. */
    std::shared_ptr<AudioServiceImpl> _service = nullptr;
    /** @brief Whether `_backend` was given by the caller: it is then kept across shutdown and init. */
    bool _injectedBackend = false;
};

}  // namespace audio
