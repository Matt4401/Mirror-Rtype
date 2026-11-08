#pragma once

#include <cstddef>
#include <string>

#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"

namespace audio {

/**
 * @brief Low-level audio output used by the audio system.
 *
 * Hides the audio library from the rest of the engine, so a muted or fake backend can replace it
 * (headless builds, tests). Every method must be safe to call when no audio device is available.
 */
class IAudioBackend {
  public:
    /** @brief Virtual destructor releasing every loaded resource. */
    virtual ~IAudioBackend() = default;

    /** @brief Deleted copy constructor: a backend owns unique audio resources. */
    IAudioBackend(const IAudioBackend&) = delete;
    /** @brief Deleted copy assignment operator. */
    IAudioBackend& operator=(const IAudioBackend&) = delete;
    /** @brief Deleted move constructor. */
    IAudioBackend(IAudioBackend&&) = delete;
    /** @brief Deleted move assignment operator. */
    IAudioBackend& operator=(IAudioBackend&&) = delete;

    /**
     * @brief Tells whether sound is actually output.
     * @return False when the backend runs muted (no device, or a null backend).
     */
    [[nodiscard]] virtual bool isAvailable() const noexcept = 0;

    /**
     * @brief Loads a sound effect fully in memory.
     *
     * @param path Audio file path (wav, ogg, mp3, flac, ...).
     * @return The new handle, or `NO_SOUND` if the file could not be loaded or the backend is muted.
     */
    [[nodiscard]] virtual SoundId loadSound(const std::string& path) = 0;

    /**
     * @brief Opens a music stream, decoded progressively while it plays.
     *
     * @param path Audio file path (ogg, mp3, wav, flac, xm, mod, ...).
     * @return The new handle, or `NO_MUSIC` if the file could not be opened or the backend is muted.
     */
    [[nodiscard]] virtual MusicId loadMusic(const std::string& path) = 0;

    /**
     * @brief Loads a sound effect, giving back `preferred` if that handle is free.
     *
     * Used by the asset manager so that a sound loaded again keeps the handle components hold. The default ignores
     * `preferred`.
     * @param path Audio file path.
     * @param preferred Handle the sound had before it was released, or `NO_SOUND`.
     * @return The handle, or `NO_SOUND`.
     */
    [[nodiscard]] virtual SoundId loadSoundAs(const std::string& path, SoundId preferred) {
        static_cast<void>(preferred);
        return loadSound(path);
    }

    /**
     * @brief Opens a music stream, giving back `preferred` if that handle is free.
     * @param path Audio file path.
     * @param preferred Handle the music had before it was released, or `NO_MUSIC`.
     * @return The handle, or `NO_MUSIC`.
     */
    [[nodiscard]] virtual MusicId loadMusicAs(const std::string& path, MusicId preferred) {
        static_cast<void>(preferred);
        return loadMusic(path);
    }

    /**
     * @brief Keeps every sound handle up to `last` for loads that ask for it as `preferred`. The default does nothing.
     * @param last Highest handle to keep.
     */
    virtual void reserveSounds(SoundId last) { static_cast<void>(last); }

    /**
     * @brief Keeps every music handle up to `last` for loads that ask for it as `preferred`. The default does nothing.
     * @param last Highest handle to keep.
     */
    virtual void reserveMusics(MusicId last) { static_cast<void>(last); }

    /**
     * @brief Releases a sound effect, e.g. when leaving the level that used it.
     *
     * Its playing instances stop and the handle becomes invalid. A handle is only given back to the same sound
     * (`loadSoundAs`), so a stale `SoundId` can never play another sound. Unknown or already unloaded handles are
     * ignored.
     *
     * @param id Handle returned by `loadSound`.
     */
    virtual void unloadSound(SoundId id) = 0;

    /**
     * @brief Releases a music stream; stops it first if it is the current music.
     *
     * A handle is only given back to the same music (`loadMusicAs`). Unknown or already unloaded handles are ignored.
     *
     * @param id Handle returned by `loadMusic`.
     */
    virtual void unloadMusic(MusicId id) = 0;

    /**
     * @brief Starts a new instance of a sound effect.
     *
     * When the voice limits are reached, the oldest playing instance is stopped to make room.
     *
     * @param id Handle returned by `loadSound`.
     * @param params Volume, panning and pitch of this instance.
     * @return A handle to control this instance, or `NO_VOICE` if the sound is unknown or the backend is muted.
     */
    virtual VoiceId playSound(SoundId id, const SoundParams& params) = 0;

    /**
     * @brief Starts a new instance of a sound only if a voice is free, without stopping any other instance.
     *
     * Used to resume a looping sound whose voice was stolen, so that sounds competing for too few voices do not
     * keep cutting each other.
     *
     * @param id Handle returned by `loadSound`.
     * @param params Volume, panning and pitch of this instance.
     * @return A handle to control this instance, or `NO_VOICE` if no voice is free, the sound is unknown or the
     * backend is muted.
     */
    virtual VoiceId playSoundWithoutStealing(SoundId id, const SoundParams& params) = 0;

    /**
     * @brief Changes the volume, panning and pitch of a playing instance, e.g. to follow a moving emitter.
     *
     * @param voice Handle returned by `playSound`.
     * @param params New settings, applied only if the instance is still playing.
     * @return `Playing` if the settings were applied, `Ended` if the instance finished or was stopped, `Stolen` if
     * its voice was taken by another instance.
     */
    virtual VoiceState updateVoice(VoiceId voice, const SoundParams& params) = 0;

    /**
     * @brief Stops a playing instance; does nothing if it already ended.
     * @param voice Handle returned by `playSound`.
     */
    virtual void stopVoice(VoiceId voice) = 0;

    /** @brief Stops every playing sound effect instance. */
    virtual void stopAllSounds() = 0;

    /**
     * @brief Plays a music stream, replacing the current one.
     *
     * Requesting the music that is already playing keeps it going instead of restarting it.
     *
     * @param id Handle returned by `loadMusic`.
     * @param loop Whether the music restarts when it reaches its end.
     */
    virtual void playMusic(MusicId id, bool loop) = 0;

    /** @brief Stops the current music stream, if any. */
    virtual void stopMusic() = 0;

    /**
     * @brief Sets the gain of music streams.
     * @param volume Linear gain in [`MIN_VOLUME`, `MAX_VOLUME`].
     */
    virtual void setMusicVolume(float volume) = 0;

    /**
     * @brief Sets the global output gain applied to every sound and music.
     * @param volume Linear gain in [`MIN_VOLUME`, `MAX_VOLUME`].
     */
    virtual void setMasterVolume(float volume) = 0;

    /** @brief Refills the music stream buffers; must be called once per frame. */
    virtual void update() = 0;

    /**
     * @brief Counts the sound effect instances currently playing, for metrics and tests.
     * @return A value never greater than `AudioConfig::maxActiveVoices`.
     */
    [[nodiscard]] virtual std::size_t activeVoiceCount() const = 0;

  protected:
    /** @brief Default constructor, only usable by implementations. */
    IAudioBackend() = default;
};

}  // namespace audio
