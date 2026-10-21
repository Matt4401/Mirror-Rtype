#pragma once

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "audio/IAudioBackend.hpp"
#include "audio/common/AudioConfig.hpp"
#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioTypes.hpp"
#include "audio/common/SoundParams.hpp"

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
 * overlap itself. raylib's audio device is global, so only one instance may exist at a time.
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

    /** @copydoc IAudioBackend::playSound */
    VoiceId playSound(SoundId id, const SoundParams& params) override;

    /** @copydoc IAudioBackend::updateVoice */
    bool updateVoice(VoiceId voice, const SoundParams& params) override;

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
    /** @brief One playable instance of a sound: an alias sharing the source sample data. */
    struct Voice {
        /** @brief raylib alias of the source sound. */
        Sound sound{};
        /** @brief Play order stamp; the oldest voice has the smallest value. */
        VoiceId startedAt = NO_VOICE;
    };

    /** @brief A loaded sound effect and its voices. */
    struct SoundSlot {
        /** @brief Sound owning the sample data. */
        Sound source{};
        /** @brief Aliases that actually play. */
        std::vector<Voice> voices;
    };

    /** @brief Looks a sound up, or returns nullptr if `id` is unknown. */
    [[nodiscard]] SoundSlot* findSound(SoundId id);

    /** @brief Looks a music up, or returns nullptr if `id` is unknown. */
    [[nodiscard]] Music* findMusic(MusicId id);

    /** @brief Returns the playing voice started first across all sounds, or nullptr if none plays. */
    [[nodiscard]] Voice* oldestPlayingVoice();

    /** @brief Returns the voice still playing the instance `voice`, or nullptr if it ended or was reused. */
    [[nodiscard]] Voice* findPlayingVoice(VoiceId voice);

    /** @brief Picks the voice to use for a new instance of `slot`, stopping another one if a limit is reached. */
    [[nodiscard]] Voice& acquireVoice(SoundSlot& slot);

    /** @brief Voice limits, both at least 1. */
    AudioConfig _config;
    /** @brief Whether the audio device was opened. */
    bool _available = false;
    /** @brief Loaded sounds; handle `n` is stored at index `n - 1`. */
    std::vector<SoundSlot> _sounds;
    /** @brief Loaded music streams; handle `n` is stored at index `n - 1`. */
    std::vector<Music> _musics;
    /** @brief Music currently played, or `NO_MUSIC`. */
    MusicId _currentMusic = NO_MUSIC;
    /** @brief Gain applied to music streams. */
    float _musicVolume = DEFAULT_VOLUME;
    /** @brief Incremented on every play to stamp `Voice::startedAt`, which doubles as the `VoiceId`. */
    std::uint64_t _playCounter = 0;
};

}  // namespace audio::detail
