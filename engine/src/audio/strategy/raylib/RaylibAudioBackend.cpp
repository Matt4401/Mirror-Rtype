#include "audio/strategy/raylib/RaylibAudioBackend.hpp"

#include <raylib.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
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

namespace {

constexpr unsigned GENERATION_SHIFT = 32;
constexpr VoiceId INDEX_MASK = 0xFFFFFFFFU;

/** @brief Packs a voice index and its generation into a handle; generation 0 is never issued. */
VoiceId makeVoiceId(std::uint32_t index, std::uint32_t generation) {
    return (VoiceId{generation} << GENERATION_SHIFT) | index;
}

std::uint32_t voiceIndex(VoiceId voice) { return static_cast<std::uint32_t>(voice & INDEX_MASK); }

std::uint32_t voiceGeneration(VoiceId voice) { return static_cast<std::uint32_t>(voice >> GENERATION_SHIFT); }

/** @brief Moves a voice to its next generation, skipping 0 so a handle can never equal `NO_VOICE`. */
void advanceGeneration(std::uint32_t& generation) {
    if (++generation == 0) {
        generation = 1;
    }
}

}  // namespace

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
        for (const MusicSlot& slot : _musics) {
            if (slot.loaded) {
                UnloadMusicStream(slot.music);
            }
        }
        // Aliases borrow the sample data of their source, so they must go first.
        for (const SoundSlot& slot : _sounds) {
            for (const std::uint32_t index : slot.voices) {
                UnloadSoundAlias(_voices.at(index).sound);
            }
        }
        for (const SoundSlot& slot : _sounds) {
            if (slot.loaded) {
                UnloadSound(slot.source);
            }
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

    SoundSlot slot{.source = source, .loaded = true};
    slot.voices.reserve(_config.voicesPerSound);
    for (std::size_t i = 0; i < _config.voicesPerSound; ++i) {
        const Sound alias = LoadSoundAlias(source);
        if (!IsSoundValid(alias)) {
            break;
        }
        slot.voices.push_back(allocateVoice(alias));
    }
    if (slot.voices.empty()) {
        UnloadSound(source);
        return NO_SOUND;
    }

    _sounds.push_back(std::move(slot));
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
    _musics.push_back(MusicSlot{.music = music, .loaded = true});
    return static_cast<MusicId>(_musics.size());
}

void RaylibAudioBackend::unloadSound(SoundId id) {
    SoundSlot* slot = findSound(id);
    if (slot == nullptr) {
        return;
    }
    for (const std::uint32_t index : slot->voices) {
        Voice& voice = _voices.at(index);
        StopSound(voice.sound);
        UnloadSoundAlias(voice.sound);
        // The new generation invalidates every handle to this voice, which will be reused by another sound.
        voice = Voice{.generation = voice.generation};
        advanceGeneration(voice.generation);
        _freeVoices.push_back(index);
    }
    UnloadSound(slot->source);
    *slot = SoundSlot{};
}

void RaylibAudioBackend::unloadMusic(MusicId id) {
    MusicSlot* slot = findMusic(id);
    if (slot == nullptr) {
        return;
    }
    if (_currentMusic == id) {
        stopMusic();
    }
    UnloadMusicStream(slot->music);
    *slot = MusicSlot{};
}

VoiceId RaylibAudioBackend::playSound(SoundId id, const SoundParams& params) {
    const SoundSlot* slot = findSound(id);
    if (!_available || slot == nullptr) {
        return NO_VOICE;
    }
    pruneActiveVoices();
    return startVoice(acquireVoice(*slot), params);
}

VoiceId RaylibAudioBackend::playSoundWithoutStealing(SoundId id, const SoundParams& params) {
    const SoundSlot* slot = findSound(id);
    if (!_available || slot == nullptr) {
        return NO_VOICE;
    }
    pruneActiveVoices();
    if (_activeVoices.size() >= _config.maxActiveVoices) {
        return NO_VOICE;
    }
    const std::optional<std::uint32_t> idle = idleVoice(*slot);
    return idle ? startVoice(*idle, params) : NO_VOICE;
}

VoiceState RaylibAudioBackend::updateVoice(VoiceId voice, const SoundParams& params) {
    const Voice* instance = findVoice(voice);
    const std::uint32_t generation = voiceGeneration(voice);
    if (instance == nullptr || generation == 0 || generation > instance->generation) {
        return VoiceState::Ended;
    }
    if (generation == instance->generation) {
        if (IsSoundPlaying(instance->sound)) {
            applyParams(instance->sound, params);
            return VoiceState::Playing;
        }
        return instance->currentStolen ? VoiceState::Stolen : VoiceState::Ended;
    }
    if (generation + 1 == instance->generation) {
        return instance->previousStolen ? VoiceState::Stolen : VoiceState::Ended;
    }
    // Replaced several times since: the voice is in high demand, so report it as taken.
    return VoiceState::Stolen;
}

void RaylibAudioBackend::stopVoice(VoiceId voice) {
    const Voice* instance = findVoice(voice);
    if (instance != nullptr && voiceGeneration(voice) == instance->generation) {
        StopSound(instance->sound);
    }
}

void RaylibAudioBackend::stopAllSounds() {
    for (const ActiveVoice& active : _activeVoices) {
        if (isActive(active)) {
            StopSound(_voices.at(active.index).sound);
        }
    }
    _activeVoices.clear();
}

void RaylibAudioBackend::playMusic(MusicId id, bool loop) {
    MusicSlot* slot = findMusic(id);
    if (!_available || slot == nullptr) {
        return;
    }
    if (_currentMusic == id && IsMusicStreamPlaying(slot->music)) {
        slot->music.looping = loop;
        return;
    }

    stopMusic();
    slot->music.looping = loop;
    SetMusicVolume(slot->music, _musicVolume);
    PlayMusicStream(slot->music);
    _currentMusic = id;
}

void RaylibAudioBackend::stopMusic() {
    if (const MusicSlot* slot = findMusic(_currentMusic); slot != nullptr) {
        StopMusicStream(slot->music);
    }
    _currentMusic = NO_MUSIC;
}

void RaylibAudioBackend::setMusicVolume(float volume) {
    _musicVolume = clampFinite(volume, DEFAULT_VOLUME, MIN_VOLUME, MAX_VOLUME);
    if (const MusicSlot* slot = findMusic(_currentMusic); slot != nullptr) {
        SetMusicVolume(slot->music, _musicVolume);
    }
}

void RaylibAudioBackend::setMasterVolume(float volume) {
    if (_available) {
        SetMasterVolume(clampFinite(volume, DEFAULT_VOLUME, MIN_VOLUME, MAX_VOLUME));
    }
}

void RaylibAudioBackend::update() {
    if (const MusicSlot* slot = findMusic(_currentMusic); slot != nullptr) {
        UpdateMusicStream(slot->music);
    }
}

std::size_t RaylibAudioBackend::activeVoiceCount() const {
    return static_cast<std::size_t>(
        std::ranges::count_if(_activeVoices, [this](const ActiveVoice& active) { return isActive(active); }));
}

RaylibAudioBackend::SoundSlot* RaylibAudioBackend::findSound(SoundId id) {
    if (id == NO_SOUND || id > _sounds.size()) {
        return nullptr;
    }
    SoundSlot& slot = _sounds.at(id - 1);
    return slot.loaded ? &slot : nullptr;
}

RaylibAudioBackend::MusicSlot* RaylibAudioBackend::findMusic(MusicId id) {
    if (id == NO_MUSIC || id > _musics.size()) {
        return nullptr;
    }
    MusicSlot& slot = _musics.at(id - 1);
    return slot.loaded ? &slot : nullptr;
}

RaylibAudioBackend::Voice* RaylibAudioBackend::findVoice(VoiceId voice) {
    const std::uint32_t index = voiceIndex(voice);
    if (voice == NO_VOICE || index >= _voices.size()) {
        return nullptr;
    }
    return &_voices.at(index);
}

bool RaylibAudioBackend::isActive(const ActiveVoice& active) const {
    const Voice& voice = _voices.at(active.index);
    return voice.generation == active.generation && IsSoundPlaying(voice.sound);
}

void RaylibAudioBackend::pruneActiveVoices() {
    std::erase_if(_activeVoices, [this](const ActiveVoice& active) { return !isActive(active); });
}

std::optional<std::uint32_t> RaylibAudioBackend::idleVoice(const SoundSlot& slot) const {
    const auto idle = std::ranges::find_if(
        slot.voices, [this](std::uint32_t index) { return !IsSoundPlaying(_voices.at(index).sound); });
    return idle != slot.voices.end() ? std::optional<std::uint32_t>(*idle) : std::nullopt;
}

std::uint32_t RaylibAudioBackend::acquireVoice(const SoundSlot& slot) {
    if (const std::optional<std::uint32_t> idle = idleVoice(slot)) {
        // Global limit reached: the oldest instance, first in the pruned list, makes room.
        if (_activeVoices.size() >= _config.maxActiveVoices) {
            Voice& victim = _voices.at(_activeVoices.front().index);
            StopSound(victim.sound);
            victim.currentStolen = true;
            _activeVoices.erase(_activeVoices.begin());
        }
        return *idle;
    }
    // Every voice of this sound is busy: restart its oldest one.
    return *std::ranges::min_element(slot.voices, {},
                                     [this](std::uint32_t index) { return _voices.at(index).startOrder; });
}

std::uint32_t RaylibAudioBackend::allocateVoice(const Sound& alias) {
    if (!_freeVoices.empty()) {
        const std::uint32_t index = _freeVoices.back();
        _freeVoices.pop_back();
        _voices.at(index).sound = alias;
        return index;
    }
    _voices.push_back(Voice{.sound = alias});
    return static_cast<std::uint32_t>(_voices.size() - 1);
}

VoiceId RaylibAudioBackend::startVoice(std::uint32_t index, const SoundParams& params) {
    Voice& voice = _voices.at(index);
    const bool cutShort = IsSoundPlaying(voice.sound) || voice.currentStolen;
    StopSound(voice.sound);
    applyParams(voice.sound, params);
    PlaySound(voice.sound);

    voice.previousStolen = cutShort;
    voice.currentStolen = false;
    advanceGeneration(voice.generation);
    voice.startOrder = ++_playCounter;
    _activeVoices.push_back(ActiveVoice{.index = index, .generation = voice.generation});
    return makeVoiceId(index, voice.generation);
}

}  // namespace audio::detail
