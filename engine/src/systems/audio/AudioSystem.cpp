#include "systems/audio/AudioSystem.hpp"

#include <algorithm>
#include <optional>
#include <utility>
#include <variant>

#include "audio/IAudioBackend.hpp"
#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioMiddleware.hpp"
#include "audio/common/AudioSpatial.hpp"
#include "audio/common/AudioTypes.hpp"
#include "audio/common/SoundParams.hpp"
#include "components/AudioListener.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/AudioEvents.hpp"
#include "events/EventBus.hpp"
#include "utils/MoveOnlyFunction.hpp"

namespace systems {

namespace {

/** @brief Subscribes `callback` to `EventType` and returns the function that undoes it. */
template <typename EventType, typename Callback>
ecs::MoveOnlyFunction<void()> subscribe(ecs::EventBus& eventBus, Callback callback) {
    const ecs::EventBus::EventToken token = eventBus.subscribe<EventType>(std::move(callback));
    return [&eventBus, token] { eventBus.unsubscribe<EventType>(token); };
}

/** @brief Returns the position of the first active `AudioListener`, or nothing if there is none. */
std::optional<audio::SoundPosition> findListener(ecs::Registry& registry) {
    std::optional<audio::SoundPosition> listener;
    registry.runSystem<ecs::AudioListener, ecs::Transform>(
        [&listener](ecs::Entity /*entity*/, const ecs::AudioListener& audioListener, const ecs::Transform& transform) {
            if (!listener && audioListener.active) {
                listener = audio::SoundPosition{.x = transform.x, .y = transform.y};
            }
        });
    return listener;
}

}  // namespace

AudioSystem::AudioSystem(audio::IAudioBackend& backend, ecs::EventBus& eventBus) : _backend(&backend) {
    // Listeners only queue: playback happens in update(), never inside the publisher's call.
    const auto queue = [this](const auto& event) { enqueue(event); };
    _unsubscribers.push_back(subscribe<events::PlaySoundEvent>(eventBus, queue));
    _unsubscribers.push_back(subscribe<events::PlayMusicEvent>(eventBus, queue));
    _unsubscribers.push_back(subscribe<events::StopMusicEvent>(eventBus, queue));
    _unsubscribers.push_back(subscribe<events::StopAllSoundsEvent>(eventBus, queue));
    _unsubscribers.push_back(subscribe<events::SetAudioVolumeEvent>(eventBus, queue));

    _backend->setMasterVolume(_masterVolume);
    _backend->setMusicVolume(_musicVolume);
}

AudioSystem::~AudioSystem() {
    for (auto& unsubscribe : _unsubscribers) {
        unsubscribe();
    }
}

void AudioSystem::update(ecs::Registry& registry) {
    _listener = findListener(registry);
    for (const AudioCommand& command : _queue) {
        std::visit([this](const auto& event) { execute(event); }, command);
    }
    _queue.clear();
    _queuedSounds = 0;

    for (const PendingSound& sound : _pendingSounds) {
        _backend->playSound(sound.id, sound.params);
    }
    _pendingSounds.clear();
    _backend->update();
}

void AudioSystem::setVolume(audio::AudioBus bus, float volume) {
    const float gain = audio::clampFinite(volume, audio::DEFAULT_VOLUME, audio::MIN_VOLUME, audio::MAX_VOLUME);
    switch (bus) {
        case audio::AudioBus::Master:
            _masterVolume = gain;
            _backend->setMasterVolume(gain);
            break;
        case audio::AudioBus::Music:
            _musicVolume = gain;
            _backend->setMusicVolume(gain);
            break;
        case audio::AudioBus::Sfx:
            _sfxVolume = gain;
            break;
    }
}

float AudioSystem::volume(audio::AudioBus bus) const noexcept {
    switch (bus) {
        case audio::AudioBus::Master:
            return _masterVolume;
        case audio::AudioBus::Music:
            return _musicVolume;
        case audio::AudioBus::Sfx:
            return _sfxVolume;
    }
    return audio::DEFAULT_VOLUME;
}

void AudioSystem::enqueue(const events::PlaySoundEvent& event) {
    if (event.sound == audio::NO_SOUND || _queuedSounds >= audio::MAX_QUEUED_SOUNDS) {
        return;
    }
    _queue.emplace_back(event);
    ++_queuedSounds;
}

void AudioSystem::enqueue(const AudioCommand& command) { _queue.push_back(command); }

void AudioSystem::execute(const events::PlaySoundEvent& event) {
    const float volume = audio::clampFinite(event.volume, audio::DEFAULT_VOLUME, audio::MIN_VOLUME, audio::MAX_VOLUME);
    audio::SoundParams params{.volume = volume * _sfxVolume, .pitch = event.pitch};
    if (event.position && _listener) {
        const audio::SpatialGains gains = audio::computeSpatial(*event.position, *_listener, _spatial);
        params.volume *= gains.volume;
        params.pan = gains.pan;
    }
    // An inaudible sound (too far, or muted bus) would only take a voice from an audible one.
    if (params.volume <= audio::MIN_VOLUME) {
        return;
    }

    // The same sound started several times in one frame would only sound louder and waste voices: keep the loudest.
    const auto same = std::ranges::find(_pendingSounds, event.sound, &PendingSound::id);
    if (same == _pendingSounds.end()) {
        _pendingSounds.push_back({.id = event.sound, .params = params});
    } else if (params.volume > same->params.volume) {
        same->params = params;
    }
}

void AudioSystem::execute(const events::PlayMusicEvent& event) {
    if (event.music != audio::NO_MUSIC) {
        _backend->playMusic(event.music, event.loop);
    }
}

void AudioSystem::execute(const events::StopMusicEvent& /*event*/) { _backend->stopMusic(); }

void AudioSystem::execute(const events::StopAllSoundsEvent& /*event*/) {
    _backend->stopAllSounds();
    _pendingSounds.clear();
}

void AudioSystem::execute(const events::SetAudioVolumeEvent& event) { setVolume(event.bus, event.volume); }

}  // namespace systems
