/**
 * @file AudioPlugin.cpp
 * @brief Implementation of AudioPlugin and its abstract service adapter.
 */

#include "audio/AudioPlugin.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "audio/IAudioBackend.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/factory/AudioBackendFactory.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "audio/spatial/SpatialConfig.hpp"
#include "audio/types/AudioTypes.hpp"
#include "core/Engine.hpp"
#include "core/Mediator.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/services/IAudioService.hpp"
#include "systems/audio/AudioSystem.hpp"

namespace audio {

/**
 * @brief Concrete adapter implementing IAudioService over the plugin's backend and AudioSystem.
 */
class AudioPlugin::AudioServiceImpl final : public engine::IAudioService {
  public:
    /**
     * @brief Constructs the adapter.
     * @param backend Audio output used to load assets.
     * @param system Audio system holding the mixer state.
     */
    AudioServiceImpl(IAudioBackend& backend, systems::AudioSystem& system) : _backend(&backend), _system(&system) {}

    /**
     * @brief Tells whether sound is actually output.
     *
     * @return true when the backend is not muted (a device is opened and a real backend is used).
     * @return false when the backend is muted (no device, or a null backend).
     */
    [[nodiscard]] bool isAvailable() const noexcept override { return _backend->isAvailable(); }

    /**
     * @brief Loads a sound effect fully in memory.
     *
     * @param path Audio file path (wav, ogg, mp3, flac, ...).
     * @return SoundId of the new sound, or `NO_SOUND` if the file could not be loaded or the backend is muted.
     */
    [[nodiscard]] SoundId loadSound(const std::string& path) override { return _backend->loadSound(path); }

    /**
     * @brief Opens a music stream, decoded progressively while it plays.
     *
     * @param path Audio file path (ogg, mp3, wav, flac, xm, mod, ...).
     * @return MusicId of the new music stream, or `NO_MUSIC` if the file could not be opened or the backend is muted.
     */
    [[nodiscard]] MusicId loadMusic(const std::string& path) override { return _backend->loadMusic(path); }

    /**
     * @brief Releases a sound effect, e.g. when leaving the level that used it.
     *
     * @param id Handle returned by `loadSound`.
     */
    void unloadSound(SoundId id) override { _backend->unloadSound(id); }

    /**
     * @brief Releases a music stream; stops it first if it is the current music.
     *
     * @param id Handle returned by `loadMusic`.
     */
    void unloadMusic(MusicId id) override { _backend->unloadMusic(id); }

    /**
     * @brief Starts a new instance of a sound effect.
     *
     * @param bus Bus to play the sound on.
     * @param volume Linear gain, clamped to [`MIN_VOLUME`, `MAX_VOLUME`].
     */
    void setVolume(AudioBus bus, float volume) override { _system->setVolume(bus, volume); }

    /**
     * @brief Reads the gain of a bus.
     *
     * @param bus Bus to read.
     * @return float Linear gain in [`MIN_VOLUME`, `MAX_VOLUME`].
     */
    [[nodiscard]] float volume(AudioBus bus) const noexcept override { return _system->volume(bus); }

    /**
     * @brief Changes how positioned sounds are panned and attenuated.
     *
     * @param config Pan and attenuation settings, in world units.
     */
    void setSpatialConfig(const SpatialConfig& config) override { _system->setSpatialConfig(config); }

    /**
     * @brief Reads the pan and attenuation settings.
     *
     * @return const SpatialConfig& Current spatial configuration.
     */
    [[nodiscard]] const SpatialConfig& spatialConfig() const noexcept override { return _system->spatialConfig(); }

    /**
     * @brief Applies the volumes and the spatial settings of a configuration.
     *
     * @param settings Settings to apply, usually read with `readAudioSettings`.
     */
    void applySettings(const AudioSettings& settings) override { _system->applySettings(settings); }

    [[nodiscard]] std::size_t activeVoiceCount() const override { return _backend->activeVoiceCount(); }

  private:
    /** @brief Wrapped backend; never null. */
    IAudioBackend* _backend;
    /** @brief Wrapped audio system; never null. */
    systems::AudioSystem* _system;
};

AudioPlugin::AudioPlugin(AudioSettings settings) : _settings(settings) {}

AudioPlugin::AudioPlugin(std::unique_ptr<IAudioBackend> backend, AudioSettings settings)
    : _settings(settings), _backend(std::move(backend)), _injectedBackend(true) {
    if (!_backend) {
        _backend = std::make_unique<NullAudioBackend>();
    }
}

AudioPlugin::~AudioPlugin() {
    _service.reset();
    _system.reset();
    _backend.reset();
}

void AudioPlugin::build(engine::PluginBuilder& builder) {
    builder.require<engine::CorePlugin>();
    builder.provides<engine::IAudioService>();
}

void AudioPlugin::init(engine::Engine& engine) {
    if (!_backend) {
        _backend = makeAudioBackend(_settings.backend, _settings.voices);
    }
    _system = std::make_unique<systems::AudioSystem>(*_backend, engine.eventBus());
    _system->applySettings(_settings);

    _service = std::make_shared<AudioServiceImpl>(*_backend, *_system);
    if (const auto res = engine.mediator().registerService<engine::IAudioService>(_service); !res) {
        _service.reset();
        _system.reset();
        throw engine::MediatorError(res.error());
    }
}

void AudioPlugin::render(engine::Engine& engine) {
    if (_system) {
        _system->update(engine.registry());
    }
}

void AudioPlugin::shutdown(engine::Engine& engine) {
    if (engine.mediator().hasService<engine::IAudioService>()) {
        engine.mediator().removeService<engine::IAudioService>();
    }
    _service.reset();
    _system.reset();
    if (_backend) {
        _backend->stopAllSounds();
        _backend->stopMusic();
    }
    if (!_injectedBackend) {
        _backend.reset();
    }
}

}  // namespace audio
