#include "audio/detail/RaylibAudioBackend.hpp"

#include <raylib.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "audio/AudioTypes.hpp"

namespace audio::detail {

namespace {

constexpr float MIN_PITCH = 0.1F;
constexpr float MAX_PITCH = 4.0F;

/** @brief Set while a backend owns raylib's global audio device. */
std::atomic_bool& deviceClaimed() {
    static std::atomic_bool claimed{false};
    return claimed;
}

/** @brief Clamps `value` to [min, max], replacing NaN and infinities by `fallback`. */
float sanitize(float value, float fallback, float min, float max) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, min, max);
}

}  // namespace

RaylibAudioBackend::RaylibAudioBackend(AudioConfig config) : _config(config) {
    _config.voicesPerSound = std::max<std::size_t>(_config.voicesPerSound, 1);
    _config.maxActiveVoices = std::max<std::size_t>(_config.maxActiveVoices, 1);
    if (deviceClaimed().exchange(true)) {
        throw std::logic_error("Only one RaylibAudioBackend may exist at a time");
    }
    // raylib does not report failures: it logs a warning and leaves the device closed, so check it explicitly.
    InitAudioDevice();
    _available = IsAudioDeviceReady();
}

RaylibAudioBackend::~RaylibAudioBackend() {
    // Known raylib 6.0 leaks, bounded and only at teardown: UnloadSoundAlias keeps 64 bytes per alias
    // (fixed upstream in raysan5/raylib#5857) and UnloadMusicStream keeps the decoder of WAV music (prefer OGG).
    if (_available) {
        for (const Music& music : _musics) {
            UnloadMusicStream(music);
        }
        // Aliases borrow the source sample data, so they must go before their source.
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

bool RaylibAudioBackend::playSound(SoundId id, const SoundParams& params) {
    SoundSlot* slot = findSound(id);
    if (!_available || slot == nullptr) {
        return false;
    }

    Voice& voice = acquireVoice(*slot);
    StopSound(voice.sound);
    SetSoundVolume(voice.sound, sanitize(params.volume, 1.0F, 0.0F, 1.0F));
    SetSoundPan(voice.sound, sanitize(params.pan, 0.0F, -1.0F, 1.0F));
    SetSoundPitch(voice.sound, sanitize(params.pitch, 1.0F, MIN_PITCH, MAX_PITCH));
    PlaySound(voice.sound);
    voice.startedAt = ++_playCounter;
    return true;
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
    _musicVolume = sanitize(volume, 1.0F, 0.0F, 1.0F);
    if (const Music* music = findMusic(_currentMusic); music != nullptr) {
        SetMusicVolume(*music, _musicVolume);
    }
}

void RaylibAudioBackend::setMasterVolume(float volume) {
    if (_available) {
        SetMasterVolume(sanitize(volume, 1.0F, 0.0F, 1.0F));
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

RaylibAudioBackend::Voice& RaylibAudioBackend::acquireVoice(SoundSlot& slot) {
    const auto idle =
        std::ranges::find_if(slot.voices, [](const Voice& voice) { return !IsSoundPlaying(voice.sound); });
    if (idle == slot.voices.end()) {
        // Every voice of this sound is busy: restart the oldest one, the active count does not change.
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
