#include "assets/audio/AudioAssetsPlugin.hpp"

#include <string>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPlugin.hpp"
#include "audio/AudioPlugin.hpp"
#include "audio/IAudioBackend.hpp"
#include "core/AudioHandles.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"

namespace assets {

void AudioAssetsPlugin::build(engine::PluginBuilder& builder) {
    builder.require<audio::AudioPlugin>();
    builder.require<AssetPlugin>();
}

void AudioAssetsPlugin::init(engine::Engine& engine) {
    audio::IAudioBackend* backend = engine.getPlugin<audio::AudioPlugin>().backend();
    if (backend == nullptr || !backend->isAvailable()) {
        return;
    }
    auto& manager = engine.registry().getResource<AssetManager>();
    manager.setLoader<Sound>(AssetLoader<Sound>{
        .load = [backend](const std::string& path, const LoadOptions& /*options*/,
                          const audio::SoundId preferred) { return backend->loadSoundAs(path, preferred); },
        .unload = [backend](const audio::SoundId id) { backend->unloadSound(id); },
        .fallback = {},
        .reserve = [backend](const audio::SoundId last) { backend->reserveSounds(last); },
    });
    manager.setLoader<Music>(AssetLoader<Music>{
        .load = [backend](const std::string& path, const LoadOptions& /*options*/,
                          const audio::MusicId preferred) { return backend->loadMusicAs(path, preferred); },
        .unload = [backend](const audio::MusicId id) { backend->unloadMusic(id); },
        .fallback = {},
        .reserve = [backend](const audio::MusicId last) { backend->reserveMusics(last); },
    });
    _attached = true;
}

void AudioAssetsPlugin::shutdown(engine::Engine& engine) {
    if (!_attached) {
        return;
    }
    _attached = false;
    if (!engine.registry().hasResource<AssetManager>()) {
        return;
    }
    auto& manager = engine.registry().getResource<AssetManager>();
    manager.detachLoader<Sound>();
    manager.detachLoader<Music>();
}

}  // namespace assets
