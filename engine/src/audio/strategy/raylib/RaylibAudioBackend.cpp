#include "audio/strategy/raylib/RaylibAudioBackend.hpp"

#include <raylib.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "audio/AudioMiddleware.hpp"
#include "audio/strategy/raylib/RaylibAudioDevice.hpp"
#include "audio/strategy/raylib/RaylibSoundParams.hpp"
#include "audio/types/AudioConfig.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"

namespace audio::detail {

RaylibAudioBackend::RaylibAudioBackend(AudioConfig config) : _config(config) {
    _config.voicesPerSound = std::max<std::size_t>(_config.voicesPerSound, 1);
    _config.maxActiveVoices = std::max<std::size_t>(_config.maxActiveVoices, 1);
    if (deviceClaimed().exchange(true)) {
        throw std::logic_error("Only one RaylibAudioBackend may exist at a time");
    }
    InitAudioDevice();
    _available = IsAudioDeviceReady();
}

RaylibAudioBackend::~RaylibAudioBackend() {
    if (_available) {
        for (const Music& music : _musics) {
            UnloadMusicStream(music);
        }
        for (const SoundSlot& slot : _sounds) {
            for (const Voice& voice : slot.voices) {
                UnloadSoundAlias(voice.sound);
            }
            UnloadSound(slot.source);
        }
        CloseAudioDevice();
    }
    deviceClaimed().store(false);
}

bool RaylibAudioBackend::isAvailable() const noexcept { return _available; }

SoundId RaylibAudioBackend::loadSound(const std::string& path) {
    if (!_available) {
        return NO_SOUND;
    }
    const Sound source = LoadSound(path.c_str());
    if (!IsSoundValid(source)) {
        return NO_SOUND;
    }

    std::vector<Voice> voices;
    voices.reserve(_config.voicesPerSound);
    for (std::size_t i = 0; i < _config.voicesPerSound; ++i) {
        const Sound alias = LoadSoundAlias(source);
        if (!IsSoundValid(alias)) {
            break;
        }
        voices.push_back(Voice{.sound = alias});
    }
    if (voices.empty()) {
        UnloadSound(source);
        return NO_SOUND;
    }

    _sounds.push_back(SoundSlot{.source = source, .voices = std::move(voices)});
    return static_cast<SoundId>(_sounds.size());
}

MusicId RaylibAudioBackend::loadMusic(const std::string& path) {
    if (!_available) {
        return NO_MUSIC;
    }
    const Music music = LoadMusicStream(path.c_str());
    if (!IsMusicValid(music)) {
        return NO_MUSIC;
    }
    _musics.push_back(music);
    return static_cast<MusicId>(_musics.size());
}

VoiceId RaylibAudioBackend::playSound(SoundId id, const SoundParams& params) {
    SoundSlot* slot = findSound(id);
    if (!_available || slot == nullptr) {
        return NO_VOICE;
    }

    return startVoice(acquireVoice(*slot), params);
}

VoiceId RaylibAudioBackend::playSoundWithoutStealing(SoundId id, const SoundParams& params) {
    SoundSlot* slot = findSound(id);
    if (!_available || slot == nullptr || activeVoiceCount() >= _config.maxActiveVoices) {
        return NO_VOICE;
    }
    const auto idle =
        std::ranges::find_if(slot->voices, [](const Voice& voice) { return !IsSoundPlaying(voice.sound); });
    if (idle == slot->voices.end()) {
        return NO_VOICE;
    }
    return startVoice(*idle, params);
}

VoiceId RaylibAudioBackend::startVoice(Voice& voice, const SoundParams& params) {
    StopSound(voice.sound);
    applyParams(voice.sound, params);
    PlaySound(voice.sound);
    voice.startedAt = ++_playCounter;
    return voice.startedAt;
}

VoiceState RaylibAudioBackend::updateVoice(VoiceId voice, const SoundParams& params) {
    const Voice* instance = findVoice(voice);
    if (instance == nullptr) {
        // No voice carries this stamp any more: another play reused it, unless the handle was never valid.
        return voice == NO_VOICE || voice > _playCounter ? VoiceState::Ended : VoiceState::Stolen;
    }
    if (!IsSoundPlaying(instance->sound)) {
        return VoiceState::Ended;
    }
    applyParams(instance->sound, params);
    return VoiceState::Playing;
}

void RaylibAudioBackend::stopVoice(VoiceId voice) {
    if (const Voice* playing = findPlayingVoice(voice); playing != nullptr) {
        StopSound(playing->sound);
    }
}

void RaylibAudioBackend::stopAllSounds() {
    for (const SoundSlot& slot : _sounds) {
        for (const Voice& voice : slot.voices) {
            StopSound(voice.sound);
        }
    }
}

void RaylibAudioBackend::playMusic(MusicId id, bool loop) {
    Music* music = findMusic(id);
    if (!_available || music == nullptr) {
        return;
    }
    if (_currentMusic == id && IsMusicStreamPlaying(*music)) {
        music->looping = loop;
        return;
    }

    stopMusic();
    music->looping = loop;
    SetMusicVolume(*music, _musicVolume);
    PlayMusicStream(*music);
    _currentMusic = id;
}

void RaylibAudioBackend::stopMusic() {
    if (const Music* music = findMusic(_currentMusic); music != nullptr) {
        StopMusicStream(*music);
    }
    _currentMusic = NO_MUSIC;
}

void RaylibAudioBackend::setMusicVolume(float volume) {
    _musicVolume = clampFinite(volume, DEFAULT_VOLUME, MIN_VOLUME, MAX_VOLUME);
    if (const Music* music = findMusic(_currentMusic); music != nullptr) {
        SetMusicVolume(*music, _musicVolume);
    }
}

void RaylibAudioBackend::setMasterVolume(float volume) {
    if (_available) {
        SetMasterVolume(clampFinite(volume, DEFAULT_VOLUME, MIN_VOLUME, MAX_VOLUME));
    }
}

void RaylibAudioBackend::update() {
    if (const Music* music = findMusic(_currentMusic); music != nullptr) {
        UpdateMusicStream(*music);
    }
}

std::size_t RaylibAudioBackend::activeVoiceCount() const {
    std::size_t count = 0;
    for (const SoundSlot& slot : _sounds) {
        count += static_cast<std::size_t>(
            std::ranges::count_if(slot.voices, [](const Voice& voice) { return IsSoundPlaying(voice.sound); }));
    }
    return count;
}

RaylibAudioBackend::SoundSlot* RaylibAudioBackend::findSound(SoundId id) {
    if (id == NO_SOUND || id > _sounds.size()) {
        return nullptr;
    }
    return &_sounds.at(id - 1);
}

Music* RaylibAudioBackend::findMusic(MusicId id) {
    if (id == NO_MUSIC || id > _musics.size()) {
        return nullptr;
    }
    return &_musics.at(id - 1);
}

RaylibAudioBackend::Voice* RaylibAudioBackend::oldestPlayingVoice() {
    Voice* oldest = nullptr;
    for (SoundSlot& slot : _sounds) {
        for (Voice& voice : slot.voices) {
            if (IsSoundPlaying(voice.sound) && (oldest == nullptr || voice.startedAt < oldest->startedAt)) {
                oldest = &voice;
            }
        }
    }
    return oldest;
}

RaylibAudioBackend::Voice* RaylibAudioBackend::findVoice(VoiceId voice) {
    if (voice == NO_VOICE) {
        return nullptr;
    }
    // A reused voice gets a new stamp, so an old handle can never control the instance that replaced it.
    for (SoundSlot& slot : _sounds) {
        for (Voice& candidate : slot.voices) {
            if (candidate.startedAt == voice) {
                return &candidate;
            }
        }
    }
    return nullptr;
}

RaylibAudioBackend::Voice* RaylibAudioBackend::findPlayingVoice(VoiceId voice) {
    Voice* candidate = findVoice(voice);
    return candidate != nullptr && IsSoundPlaying(candidate->sound) ? candidate : nullptr;
}

RaylibAudioBackend::Voice& RaylibAudioBackend::acquireVoice(SoundSlot& slot) {
    const auto idle =
        std::ranges::find_if(slot.voices, [](const Voice& voice) { return !IsSoundPlaying(voice.sound); });

    if (idle == slot.voices.end()) {
        return *std::ranges::min_element(slot.voices, {}, &Voice::startedAt);
    }
    if (activeVoiceCount() >= _config.maxActiveVoices) {
        if (const Voice* victim = oldestPlayingVoice(); victim != nullptr) {
            StopSound(victim->sound);
        }
    }
    return *idle;
}

}  // namespace audio::detail
