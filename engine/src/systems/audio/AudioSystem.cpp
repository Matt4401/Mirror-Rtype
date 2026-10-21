#include "systems/audio/AudioSystem.hpp"

#include <algorithm>
#include <utility>
#include <variant>

#include "audio/IAudioBackend.hpp"
#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioMiddleware.hpp"
#include "audio/common/AudioTypes.hpp"
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

void AudioSystem::update() {
    _playedThisUpdate.clear();
    for (const AudioCommand& command : _queue) {
        std::visit([this](const auto& event) { execute(event); }, command);
    }
    _queue.clear();
    _queuedSounds = 0;
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
    // The same sound started several times in one frame would only sound louder and waste voices.
    if (std::ranges::contains(_playedThisUpdate, event.sound)) {
        return;
    }
    _playedThisUpdate.push_back(event.sound);

    const float volume = audio::clampFinite(event.volume, audio::DEFAULT_VOLUME, audio::MIN_VOLUME, audio::MAX_VOLUME);
    _backend->playSound(event.sound, {.volume = volume * _sfxVolume, .pitch = event.pitch});
}

void AudioSystem::execute(const events::PlayMusicEvent& event) {
    if (event.music != audio::NO_MUSIC) {
        _backend->playMusic(event.music, event.loop);
    }
}

void AudioSystem::execute(const events::StopMusicEvent& /*event*/) { _backend->stopMusic(); }

void AudioSystem::execute(const events::StopAllSoundsEvent& /*event*/) {
    _backend->stopAllSounds();
    _playedThisUpdate.clear();
}

void AudioSystem::execute(const events::SetAudioVolumeEvent& event) { setVolume(event.bus, event.volume); }

}  // namespace systems
