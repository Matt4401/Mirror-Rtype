/**
 * @file IAudioService.hpp
 * @brief Abstract service interface for audio assets and mixer settings.
 */

#pragma once

#include <cstddef>
#include <string>

#include "audio/settings/AudioSettings.hpp"
#include "audio/spatial/SpatialConfig.hpp"
#include "audio/types/AudioTypes.hpp"

namespace engine {

/**
 * @brief Abstract audio service exposed through the Mediator.
 *
 * Loads the sounds and musics and controls the mixer. Playback itself is requested by publishing audio events
 * (`PlaySoundEvent`, `PlayMusicEvent`, ...) on the `EventBus`, so gameplay code never needs this service to play
 * a sound and keeps working when no audio plugin is installed.
 */
class IAudioService {
  public:
    /** @brief Default constructor. */
    IAudioService() = default;

    /** @brief Virtual destructor. */
    virtual ~IAudioService() = default;

    /** @brief Default copy constructor. */
    IAudioService(const IAudioService&) = default;

    /** @brief Default copy assignment operator. */
    IAudioService& operator=(const IAudioService&) = default;

    /** @brief Default move constructor. */
    IAudioService(IAudioService&&) noexcept = default;

    /** @brief Default move assignment operator. */
    IAudioService& operator=(IAudioService&&) noexcept = default;

    /**
     * @brief Tells whether sound is actually output.
     * @return False when the audio runs muted (no device, or a null backend).
     */
    [[nodiscard]] virtual bool isAvailable() const noexcept = 0;

    /**
     * @brief Loads a sound effect fully in memory.
     * @param path Audio file path.
     * @return The new handle, or `NO_SOUND` if the file could not be loaded or the audio is muted.
     */
    [[nodiscard]] virtual audio::SoundId loadSound(const std::string& path) = 0;

    /**
     * @brief Opens a music stream.
     * @param path Audio file path.
     * @return The new handle, or `NO_MUSIC` if the file could not be opened or the audio is muted.
     */
    [[nodiscard]] virtual audio::MusicId loadMusic(const std::string& path) = 0;

    /**
     * @brief Releases a sound effect; unknown handles are ignored.
     * @param id Handle returned by `loadSound`.
     */
    virtual void unloadSound(audio::SoundId id) = 0;

    /**
     * @brief Releases a music stream; unknown handles are ignored.
     * @param id Handle returned by `loadMusic`.
     */
    virtual void unloadMusic(audio::MusicId id) = 0;

    /**
     * @brief Sets the gain of a bus immediately.
     * @param bus Bus to change.
     * @param volume Linear gain, clamped to [`MIN_VOLUME`, `MAX_VOLUME`].
     */
    virtual void setVolume(audio::AudioBus bus, float volume) = 0;

    /**
     * @brief Reads the gain of a bus.
     * @param bus Bus to read.
     * @return Linear gain in [`MIN_VOLUME`, `MAX_VOLUME`].
     */
    [[nodiscard]] virtual float volume(audio::AudioBus bus) const noexcept = 0;

    /**
     * @brief Changes how positioned sounds are panned and attenuated.
     * @param config Pan and attenuation settings, in world units.
     */
    virtual void setSpatialConfig(const audio::SpatialConfig& config) = 0;

    /**
     * @brief Reads the pan and attenuation settings.
     * @return Current spatial configuration.
     */
    [[nodiscard]] virtual const audio::SpatialConfig& spatialConfig() const noexcept = 0;

    /**
     * @brief Applies the volumes and spatial settings of a configuration at runtime.
     *
     * The backend kind and voice limits are ignored: they only matter when the plugin is created.
     *
     * @param settings Settings to apply.
     */
    virtual void applySettings(const audio::AudioSettings& settings) = 0;

    /**
     * @brief Counts the sound effect instances currently playing, for metrics and debugging.
     * @return Number of active voices.
     */
    [[nodiscard]] virtual std::size_t activeVoiceCount() const = 0;
};

}  // namespace engine
