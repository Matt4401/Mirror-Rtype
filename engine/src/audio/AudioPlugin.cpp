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

    [[nodiscard]] bool isAvailable() const noexcept override { return _backend->isAvailable(); }

    [[nodiscard]] SoundId loadSound(const std::string& path) override { return _backend->loadSound(path); }

    [[nodiscard]] MusicId loadMusic(const std::string& path) override { return _backend->loadMusic(path); }

    void unloadSound(SoundId id) override { _backend->unloadSound(id); }

    void unloadMusic(MusicId id) override { _backend->unloadMusic(id); }

    void setVolume(AudioBus bus, float volume) override { _system->setVolume(bus, volume); }

    [[nodiscard]] float volume(AudioBus bus) const noexcept override { return _system->volume(bus); }

    void setSpatialConfig(const SpatialConfig& config) override { _system->setSpatialConfig(config); }

    [[nodiscard]] const SpatialConfig& spatialConfig() const noexcept override { return _system->spatialConfig(); }

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
