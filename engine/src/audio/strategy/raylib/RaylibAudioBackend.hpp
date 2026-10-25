#pragma once

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "audio/IAudioBackend.hpp"
#include "audio/types/AudioConfig.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"

namespace audio::detail {

/**
 * @brief Audio backend built on raylib (miniaudio): owns the audio device, sound effects and music streams.
 *
 * Private to the `engine_audio` module because it exposes raylib types; the rest of the engine goes through
 * `IAudioBackend` and `makeAudioBackend()`.
 *
 * The device is opened by the constructor and closed by the destructor, after every resource has been unloaded.
 * If no device can be opened (no sound card, CI, Docker), the backend stays usable in muted mode:
 * `isAvailable()` returns false and every call is a no-op.
 *
 * Each loaded sound gets `AudioConfig::voicesPerSound` aliases sharing its sample data, so the same sound can
 * overlap itself. Controlling an instance is O(1); starting one costs O(active voices + voices per sound), never a
 * walk over every loaded sound. raylib's audio device is global, so only one instance may exist at a time.
 * Not thread-safe: use it from a single thread.
 */
class RaylibAudioBackend final : public IAudioBackend {
  public:
    /**
     * @brief Opens the default audio device, or falls back to muted mode if none is available.
     *
     * @param config Voice limits.
     * @throws std::logic_error If another `RaylibAudioBackend` already exists.
     */
    explicit RaylibAudioBackend(AudioConfig config = AudioConfig{});

    /** @brief Unloads every sound and music, then closes the audio device. */
    ~RaylibAudioBackend() override;

    /** @brief Deleted copy constructor: the audio device is a unique resource. */
    RaylibAudioBackend(const RaylibAudioBackend&) = delete;
    /** @brief Deleted copy assignment operator. */
    RaylibAudioBackend& operator=(const RaylibAudioBackend&) = delete;
    /** @brief Deleted move constructor. */
    RaylibAudioBackend(RaylibAudioBackend&&) = delete;
    /** @brief Deleted move assignment operator. */
    RaylibAudioBackend& operator=(RaylibAudioBackend&&) = delete;

    /** @copydoc IAudioBackend::isAvailable */
    [[nodiscard]] bool isAvailable() const noexcept override;

    /** @copydoc IAudioBackend::loadSound */
    [[nodiscard]] SoundId loadSound(const std::string& path) override;

    /** @copydoc IAudioBackend::loadMusic */
    [[nodiscard]] MusicId loadMusic(const std::string& path) override;

    /** @copydoc IAudioBackend::unloadSound */
    void unloadSound(SoundId id) override;

    /** @copydoc IAudioBackend::unloadMusic */
    void unloadMusic(MusicId id) override;

    /** @copydoc IAudioBackend::playSound */
    VoiceId playSound(SoundId id, const SoundParams& params) override;

    /** @copydoc IAudioBackend::playSoundWithoutStealing */
    VoiceId playSoundWithoutStealing(SoundId id, const SoundParams& params) override;

    /** @copydoc IAudioBackend::updateVoice */
    VoiceState updateVoice(VoiceId voice, const SoundParams& params) override;

    /** @copydoc IAudioBackend::stopVoice */
    void stopVoice(VoiceId voice) override;

    /** @copydoc IAudioBackend::stopAllSounds */
    void stopAllSounds() override;

    /** @copydoc IAudioBackend::playMusic */
    void playMusic(MusicId id, bool loop) override;

    /** @copydoc IAudioBackend::stopMusic */
    void stopMusic() override;

    /** @copydoc IAudioBackend::setMusicVolume */
    void setMusicVolume(float volume) override;

    /** @copydoc IAudioBackend::setMasterVolume */
    void setMasterVolume(float volume) override;

    /** @copydoc IAudioBackend::update */
    void update() override;

    /** @copydoc IAudioBackend::activeVoiceCount */
    [[nodiscard]] std::size_t activeVoiceCount() const override;

  private:
    /**
     * @brief A reusable playback slot: a raylib alias sharing the sample data of its sound.
     *
     * A `VoiceId` is the voice index plus its `generation`, so a voice is found in O(1) and a handle whose
     * generation is outdated is known to refer to an instance that was replaced.
     */
    struct Voice {
        /** @brief raylib alias of the source sound; empty once its sound is unloaded. */
        Sound sound{};
        /** @brief Incremented on every start and unload; never reset, so old handles stay invalid. */
        std::uint32_t generation = 0;
        /** @brief Start order across all voices; the oldest instance has the smallest value. */
        std::uint64_t startOrder = 0;
        /** @brief Whether the current instance was stopped to make room for another one. */
        bool currentStolen = false;
        /** @brief Whether the instance before the current one was stopped to make room for it. */
        bool previousStolen = false;
    };

    /** @brief A sound effect: its sample data and the voices that play it. */
    struct SoundSlot {
        /** @brief Sound owning the sample data. */
        Sound source{};
        /** @brief Indices of its voices in `_voices`. */
        std::vector<std::uint32_t> voices;
        /** @brief False once unloaded; the slot is kept so its handle is never given to another sound. */
        bool loaded = false;
    };

    /** @brief A music stream. */
    struct MusicSlot {
        /** @brief raylib stream. */
        Music music{};
        /** @brief False once unloaded; the slot is kept so its handle is never given to another music. */
        bool loaded = false;
    };

    /** @brief An instance started on a voice, in `_activeVoices`. */
    struct ActiveVoice {
        /** @brief Voice index in `_voices`. */
        std::uint32_t index = 0;
        /** @brief Generation of the voice when the instance started. */
        std::uint32_t generation = 0;
    };

    /** @brief Looks a loaded sound up, or returns nullptr if `id` is unknown or unloaded. */
    [[nodiscard]] SoundSlot* findSound(SoundId id);

    /** @brief Looks a loaded music up, or returns nullptr if `id` is unknown or unloaded. */
    [[nodiscard]] MusicSlot* findMusic(MusicId id);

    /** @brief Returns the voice a handle points to, whatever its generation, or nullptr for an invalid handle. */
    [[nodiscard]] Voice* findVoice(VoiceId voice);

    /** @brief Tells whether an entry of `_activeVoices` is still the instance playing on its voice. */
    [[nodiscard]] bool isActive(const ActiveVoice& active) const;

    /** @brief Removes the instances that ended or were replaced from `_activeVoices`, keeping the start order. */
    void pruneActiveVoices();

    /** @brief Returns a voice of `slot` that is not playing, if any. */
    [[nodiscard]] std::optional<std::uint32_t> idleVoice(const SoundSlot& slot) const;

    /** @brief Picks the voice for a new instance of `slot`, stopping another instance if a limit is reached. */
    [[nodiscard]] std::uint32_t acquireVoice(const SoundSlot& slot);

    /** @brief Stores an alias in a free voice, or in a new one, and returns its index. */
    [[nodiscard]] std::uint32_t allocateVoice(const Sound& alias);

    /** @brief Plays `params` on a voice and returns the handle of the new instance. */
    VoiceId startVoice(std::uint32_t index, const SoundParams& params);

    /** @brief Voice limits, both at least 1. */
    AudioConfig _config;
    /** @brief Whether the audio device was opened. */
    bool _available = false;
    /** @brief Every voice of every sound; a `VoiceId` holds an index into it. */
    std::vector<Voice> _voices;
    /** @brief Indices of voices released by unloaded sounds, reused by the next loads. */
    std::vector<std::uint32_t> _freeVoices;
    /** @brief Started instances, oldest first; may hold ended ones until the next prune. */
    std::vector<ActiveVoice> _activeVoices;
    /** @brief Sounds; handle `n` is stored at index `n - 1`. */
    std::vector<SoundSlot> _sounds;
    /** @brief Music streams; handle `n` is stored at index `n - 1`. */
    std::vector<MusicSlot> _musics;
    /** @brief Music currently played, or `NO_MUSIC`. */
    MusicId _currentMusic = NO_MUSIC;
    /** @brief Gain applied to music streams. */
    float _musicVolume = DEFAULT_VOLUME;
    /** @brief Incremented on every start to fill `Voice::startOrder`. */
    std::uint64_t _playCounter = 0;
};

}  // namespace audio::detail
