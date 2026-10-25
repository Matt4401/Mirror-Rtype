#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>

#include "audio/NullAudioBackend.hpp"
#include "audio/factory/AudioBackendFactory.hpp"
#include "audio/strategy/raylib/RaylibAudioBackend.hpp"
#include "audio/types/AudioConfig.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"

namespace {

template <typename T>
void writeLittleEndian(std::ofstream& out, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.put(static_cast<char>((static_cast<std::uint64_t>(value) >> (8 * i)) & 0xFFU));
    }
}

/** @brief Writes a mono 16-bit PCM sine wave, so the tests do not depend on asset files. */
std::filesystem::path writeSineWav(const std::string& name, float seconds) {
    constexpr std::uint32_t SAMPLE_RATE = 44100;
    constexpr std::uint16_t BITS = 16;
    constexpr float FREQUENCY = 440.0F;
    constexpr float AMPLITUDE = 8000.0F;
    const auto sampleCount = static_cast<std::uint32_t>(seconds * SAMPLE_RATE);
    const std::uint32_t dataSize = sampleCount * (BITS / 8);

    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4);
    writeLittleEndian<std::uint32_t>(out, 36 + dataSize);
    out.write("WAVEfmt ", 8);
    writeLittleEndian<std::uint32_t>(out, 16);
    writeLittleEndian<std::uint16_t>(out, 1);  // PCM
    writeLittleEndian<std::uint16_t>(out, 1);  // mono
    writeLittleEndian<std::uint32_t>(out, SAMPLE_RATE);
    writeLittleEndian<std::uint32_t>(out, SAMPLE_RATE * (BITS / 8));
    writeLittleEndian<std::uint16_t>(out, BITS / 8);
    writeLittleEndian<std::uint16_t>(out, BITS);
    out.write("data", 4);
    writeLittleEndian<std::uint32_t>(out, dataSize);
    for (std::uint32_t i = 0; i < sampleCount; ++i) {
        const float sample =
            AMPLITUDE * std::sin(2.0F * std::numbers::pi_v<float> * FREQUENCY * static_cast<float>(i) / SAMPLE_RATE);
        writeLittleEndian<std::int16_t>(out, static_cast<std::int16_t>(sample));
    }
    return path;
}

}  // namespace

TEST(RaylibAudioBackend, UnknownHandlesAreIgnored) {
    audio::detail::RaylibAudioBackend backend;

    EXPECT_EQ(backend.playSound(audio::NO_SOUND, {}), audio::NO_VOICE);
    EXPECT_EQ(backend.playSound(42, {}), audio::NO_VOICE);
    backend.playMusic(audio::NO_MUSIC, true);
    backend.playMusic(42, true);
    backend.stopMusic();
    backend.stopAllSounds();
    backend.update();
    EXPECT_EQ(backend.activeVoiceCount(), 0U);
}

TEST(RaylibAudioBackend, MissingFilesReturnInvalidHandles) {
    audio::detail::RaylibAudioBackend backend;

    EXPECT_EQ(backend.loadSound("does/not/exist.wav"), audio::NO_SOUND);
    EXPECT_EQ(backend.loadMusic("does/not/exist.ogg"), audio::NO_MUSIC);
}

TEST(RaylibAudioBackend, InvalidVolumesDoNotCrash) {
    audio::detail::RaylibAudioBackend backend;

    backend.setMasterVolume(std::numeric_limits<float>::quiet_NaN());
    backend.setMasterVolume(-3.0F);
    backend.setMusicVolume(std::numeric_limits<float>::infinity());
    backend.setMasterVolume(1.0F);
}

TEST(RaylibAudioBackend, OnlyOneInstanceAtATime) {
    {
        const audio::detail::RaylibAudioBackend backend;
        EXPECT_THROW(audio::detail::RaylibAudioBackend{}, std::logic_error);
    }
    EXPECT_NO_THROW(audio::detail::RaylibAudioBackend{});
}

TEST(RaylibAudioBackend, VoicesPerSoundAreBounded) {
    audio::detail::RaylibAudioBackend backend(audio::AudioConfig{.voicesPerSound = 2, .maxActiveVoices = 8});
    if (!backend.isAvailable()) {
        GTEST_SKIP() << "No audio device available";
    }
    const audio::SoundId sound = backend.loadSound(writeSineWav("mirror_rtype_voices.wav", 1.0F).string());
    ASSERT_NE(sound, audio::NO_SOUND);

    for (int i = 0; i < 5; ++i) {
        EXPECT_NE(backend.playSound(sound, {.volume = 0.0F}), audio::NO_VOICE);
    }
    EXPECT_EQ(backend.activeVoiceCount(), 2U);

    backend.stopAllSounds();
    EXPECT_EQ(backend.activeVoiceCount(), 0U);
}

TEST(RaylibAudioBackend, ActiveVoicesAreBoundedAcrossSounds) {
    audio::detail::RaylibAudioBackend backend(audio::AudioConfig{.voicesPerSound = 4, .maxActiveVoices = 3});
    if (!backend.isAvailable()) {
        GTEST_SKIP() << "No audio device available";
    }
    const std::string path = writeSineWav("mirror_rtype_cap.wav", 1.0F).string();
    const audio::SoundId first = backend.loadSound(path);
    const audio::SoundId second = backend.loadSound(path);
    ASSERT_NE(first, audio::NO_SOUND);
    ASSERT_NE(second, audio::NO_SOUND);

    for (int i = 0; i < 3; ++i) {
        EXPECT_NE(backend.playSound(first, {.volume = 0.0F}), audio::NO_VOICE);
        EXPECT_NE(backend.playSound(second, {.volume = 0.0F}), audio::NO_VOICE);
    }
    EXPECT_EQ(backend.activeVoiceCount(), 3U);
}

TEST(RaylibAudioBackend, MusicCanBePlayedAndStopped) {
    audio::detail::RaylibAudioBackend backend;
    if (!backend.isAvailable()) {
        GTEST_SKIP() << "No audio device available";
    }
    const audio::MusicId music = backend.loadMusic(writeSineWav("mirror_rtype_music.wav", 1.0F).string());
    ASSERT_NE(music, audio::NO_MUSIC);

    backend.setMusicVolume(0.0F);
    backend.playMusic(music, true);
    backend.playMusic(music, true);
    backend.update();
    backend.stopMusic();
    backend.update();
}

TEST(RaylibAudioBackend, VoicesCanBeUpdatedAndStopped) {
    audio::detail::RaylibAudioBackend backend(audio::AudioConfig{.voicesPerSound = 1});
    if (!backend.isAvailable()) {
        GTEST_SKIP() << "No audio device available";
    }
    const audio::SoundId sound = backend.loadSound(writeSineWav("mirror_rtype_voice.wav", 1.0F).string());
    ASSERT_NE(sound, audio::NO_SOUND);

    const audio::VoiceId first = backend.playSound(sound, {.volume = 0.0F});
    ASSERT_NE(first, audio::NO_VOICE);
    EXPECT_EQ(backend.updateVoice(first, {.volume = 0.0F, .pan = audio::PAN_LEFT}), audio::VoiceState::Playing);

    const audio::VoiceId second = backend.playSound(sound, {.volume = 0.0F});
    EXPECT_NE(second, first);
    EXPECT_EQ(backend.updateVoice(first, {.volume = 0.0F}), audio::VoiceState::Stolen)
        << "the only voice now plays the second instance";
    EXPECT_EQ(backend.updateVoice(second, {.volume = 0.0F}), audio::VoiceState::Playing);
    EXPECT_EQ(backend.playSoundWithoutStealing(sound, {.volume = 0.0F}), audio::NO_VOICE)
        << "the only voice is busy and must not be taken";

    backend.stopVoice(second);
    EXPECT_EQ(backend.updateVoice(second, {.volume = 0.0F}), audio::VoiceState::Ended);
    EXPECT_NE(backend.playSoundWithoutStealing(sound, {.volume = 0.0F}), audio::NO_VOICE);
    backend.stopAllSounds();
    EXPECT_EQ(backend.activeVoiceCount(), 0U);
}

TEST(NullAudioBackend, IsMutedAndIgnoresEverything) {
    audio::NullAudioBackend backend;

    EXPECT_FALSE(backend.isAvailable());
    EXPECT_EQ(backend.loadSound("laser.wav"), audio::NO_SOUND);
    EXPECT_EQ(backend.loadMusic("theme.ogg"), audio::NO_MUSIC);
    EXPECT_EQ(backend.playSound(1, {}), audio::NO_VOICE);
    backend.playMusic(1, true);
    backend.setMasterVolume(0.5F);
    backend.update();
    EXPECT_EQ(backend.updateVoice(1, {}), audio::VoiceState::Ended);
    EXPECT_EQ(backend.playSoundWithoutStealing(1, {}), audio::NO_VOICE);
    backend.stopVoice(1);
    EXPECT_EQ(backend.activeVoiceCount(), 0U);
}

TEST(AudioBackendFactory, AlwaysReturnsABackend) {
    const std::unique_ptr<audio::IAudioBackend> backend = audio::makeAudioBackend();

    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->playSound(audio::NO_SOUND, {}), audio::NO_VOICE);
}

TEST(AudioBackendFactory, NullKindOpensNoDevice) {
    const std::unique_ptr<audio::IAudioBackend> muted = audio::makeAudioBackend(audio::AudioBackendKind::Null);
    EXPECT_FALSE(muted->isAvailable());

    // No device was claimed, so a real backend can still be created alongside.
    EXPECT_NO_THROW(audio::detail::RaylibAudioBackend{});
}

TEST(AudioBackendFactory, ReleasesTheDeviceWithTheBackend) {
    {
        const auto backend = audio::makeAudioBackend();
    }
    EXPECT_NO_THROW(audio::makeAudioBackend());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
