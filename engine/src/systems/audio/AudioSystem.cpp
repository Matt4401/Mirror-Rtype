#include "systems/audio/AudioSystem.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <variant>

#include "audio/AudioMiddleware.hpp"
#include "audio/IAudioBackend.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "audio/spatial/AudioSpatial.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"
#include "components/AudioEmitter.hpp"
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
    for (const auto& [entity, state] : _emitters) {
        _backend->stopVoice(state.voice);
    }
}

void AudioSystem::update(ecs::Registry& registry) {
    _listener = findListener(registry);
    for (const AudioCommand& command : _queue) {
        std::visit([this](const auto& event) { execute(event); }, command);
    }
    _queue.clear();
    _queuedSounds = 0;

    refreshPlayingSounds();
    for (const PendingSound& sound : _pendingSounds) {
        const audio::VoiceId voice = _backend->playSound(sound.id, applySfxBus(sound.base));
        if (voice != audio::NO_VOICE) {
            _playingSounds.push_back({.voice = voice, .base = sound.base});
        }
    }
    _pendingSounds.clear();

    updateEmitters(registry);
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
            // Emitters pick the new gain up on the next update; one-shot sounds already playing are updated now.
            _sfxVolume = gain;
            refreshPlayingSounds();
            break;
    }
}

void AudioSystem::applySettings(const audio::AudioSettings& settings) {
    setVolume(audio::AudioBus::Master, settings.masterVolume);
    setVolume(audio::AudioBus::Music, settings.musicVolume);
    setVolume(audio::AudioBus::Sfx, settings.sfxVolume);
    setSpatialConfig(settings.spatial);
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

audio::SoundParams AudioSystem::resolveParams(float volume, float pitch,
                                              const std::optional<audio::SoundPosition>& position) const {
    const float gain = audio::clampFinite(volume, audio::DEFAULT_VOLUME, audio::MIN_VOLUME, audio::MAX_VOLUME);
    audio::SoundParams params{.volume = gain, .pitch = pitch};
    if (const std::optional<audio::SoundPosition> where = heardFrom(position); where && _listener) {
        const audio::SpatialGains gains = audio::computeSpatial(*where, *_listener, _spatial);
        params.volume *= gains.volume;
        params.pan = gains.pan;
    }
    return params;
}

audio::SoundParams AudioSystem::applySfxBus(audio::SoundParams base) const noexcept {
    base.volume *= _sfxVolume;
    return base;
}

std::optional<audio::SoundPosition> AudioSystem::heardFrom(const std::optional<audio::SoundPosition>& position) const {
    return position && _listener && _spatial.enabled ? position : std::nullopt;
}

void AudioSystem::refreshPlayingSounds() {
    std::erase_if(_playingSounds, [this](const PlayingSound& sound) {
        return _backend->updateVoice(sound.voice, applySfxBus(sound.base)) != audio::VoiceState::Playing;
    });
}

void AudioSystem::updateEmitters(ecs::Registry& registry) {
    for (auto& [entity, state] : _emitters) {
        state.seen = false;
    }

    registry.runSystem<ecs::AudioEmitter, ecs::Transform>(
        [this](ecs::Entity entity, const ecs::AudioEmitter& emitter, const ecs::Transform& transform) {
            EmitterState& state = _emitters[entity];
            state.seen = true;
            const bool active = emitter.playing && emitter.sound != audio::NO_SOUND;
            if (!active || emitter.sound != state.sound) {
                _backend->stopVoice(state.voice);
                state = EmitterState{.sound = emitter.sound, .seen = true};
                if (!active) {
                    return;
                }
            }

            const audio::SoundParams params = applySfxBus(
                resolveParams(emitter.volume, emitter.pitch, audio::SoundPosition{.x = transform.x, .y = transform.y}));
            // Still playing: move it with the entity.
            const audio::VoiceState voice = _backend->updateVoice(state.voice, params);
            if (voice == audio::VoiceState::Playing) {
                return;
            }
            if (voice == audio::VoiceState::Stolen) {
                state.stolen = true;
            }
            // Ended, stolen by another sound, or not started yet. An inaudible emitter waits to come closer.
            if ((state.started && !emitter.loop) || params.volume <= audio::MIN_VOLUME) {
                return;
            }
            // A stolen emitter must not steal back, or emitters competing for too few voices would restart each
            // other on every update. It waits for a free voice instead.
            if (state.stolen) {
                state.voice = _backend->playSoundWithoutStealing(emitter.sound, params);
                state.stolen = state.voice == audio::NO_VOICE;
            } else {
                state.voice = _backend->playSound(emitter.sound, params);
            }
            state.started = true;
        });

    // The entity was destroyed or lost its emitter: silence it.
    std::erase_if(_emitters, [this](const auto& entry) {
        if (entry.second.seen) {
            return false;
        }
        _backend->stopVoice(entry.second.voice);
        return true;
    });
}

void AudioSystem::execute(const events::PlaySoundEvent& event) {
    const audio::SoundParams base = resolveParams(event.volume, event.pitch, event.position);
    // An inaudible sound (too far, or muted bus) would only take a voice from an audible one.
    if (applySfxBus(base).volume <= audio::MIN_VOLUME) {
        return;
    }

    // The same sound started at the same place in one frame would only sound louder and waste a voice: keep the
    // loudest. Requests from different places (two players firing) are all played, each from its own side.
    const std::optional<audio::SoundPosition> where = heardFrom(event.position);
    const auto same = std::ranges::find_if(_pendingSounds, [&](const PendingSound& pending) {
        if (pending.id != event.sound || pending.position.has_value() != where.has_value()) {
            return false;
        }
        return !where ||
               std::hypot(pending.position->x - where->x, pending.position->y - where->y) <= _spatial.mergeDistance;
    });
    if (same == _pendingSounds.end()) {
        _pendingSounds.push_back({.id = event.sound, .base = base, .position = where});
    } else if (base.volume > same->base.volume) {
        same->base = base;
        same->position = where;
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
    _playingSounds.clear();
}

void AudioSystem::execute(const events::SetAudioVolumeEvent& event) { setVolume(event.bus, event.volume); }

}  // namespace systems
