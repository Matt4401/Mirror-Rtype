#pragma once

#include <cstddef>
#include <string>

#include "audio/AudioConstants.hpp"
#include "audio/AudioTypes.hpp"
#include "audio/IAudioBackend.hpp"
#include "audio/SoundParams.hpp"

namespace audio {

/**
 * @brief Muted backend: accepts every call and outputs nothing.
 *
 * Used when no audio device is available (headless, CI, Docker) and as a stand-in in tests.
 * Loading always fails, so the audio system simply ignores the sounds it is asked to play.
 */
class NullAudioBackend final : public IAudioBackend {
  public:
    /** @brief Creates a muted backend. */
    NullAudioBackend() = default;
    /** @brief Default destructor. */
    ~NullAudioBackend() override = default;

    /** @brief Deleted copy constructor. */
    NullAudioBackend(const NullAudioBackend&) = delete;
    /** @brief Deleted copy assignment operator. */
    NullAudioBackend& operator=(const NullAudioBackend&) = delete;
    /** @brief Deleted move constructor. */
    NullAudioBackend(NullAudioBackend&&) = delete;
    /** @brief Deleted move assignment operator. */
    NullAudioBackend& operator=(NullAudioBackend&&) = delete;

    /** @copydoc IAudioBackend::isAvailable */
    [[nodiscard]] bool isAvailable() const noexcept override { return false; }

    /** @copydoc IAudioBackend::loadSound */
    [[nodiscard]] SoundId loadSound(const std::string& /*path*/) override { return NO_SOUND; }

    /** @copydoc IAudioBackend::loadMusic */
    [[nodiscard]] MusicId loadMusic(const std::string& /*path*/) override { return NO_MUSIC; }

    /** @copydoc IAudioBackend::playSound */
    bool playSound(SoundId /*id*/, const SoundParams& /*params*/) override { return false; }

    /** @copydoc IAudioBackend::stopAllSounds */
    void stopAllSounds() override {}

    /** @copydoc IAudioBackend::playMusic */
    void playMusic(MusicId /*id*/, bool /*loop*/) override {}

    /** @copydoc IAudioBackend::stopMusic */
    void stopMusic() override {}

    /** @copydoc IAudioBackend::setMusicVolume */
    void setMusicVolume(float /*volume*/) override {}

    /** @copydoc IAudioBackend::setMasterVolume */
    void setMasterVolume(float /*volume*/) override {}

    /** @copydoc IAudioBackend::update */
    void update() override {}

    /** @copydoc IAudioBackend::activeVoiceCount */
    [[nodiscard]] std::size_t activeVoiceCount() const override { return 0; }
};

}  // namespace audio
