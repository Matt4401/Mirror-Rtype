#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "audio/settings/AudioSettings.hpp"
#include "audio/settings/AudioSettingsLoader.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "config/ConfigFile.hpp"

namespace {

/** @brief Reads audio settings from TOML text. */
audio::AudioSettings read(std::string_view toml, std::vector<std::string>& warnings) {
    return audio::readAudioSettings(config::ConfigFile::parse(toml), warnings);
}

/** @brief Tells whether one warning mentions `key`. */
bool mentions(const std::vector<std::string>& warnings, std::string_view key) {
    return std::ranges::any_of(warnings,
                               [key](const std::string& warning) { return warning.find(key) != std::string::npos; });
}

}  // namespace

TEST(AudioSettings, EmptyConfigurationGivesTheDefaults) {
    std::vector<std::string> warnings;
    const audio::AudioSettings settings = read("", warnings);
    const audio::AudioSettings defaults;

    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(settings.backend, audio::AudioBackendKind::Auto);
    EXPECT_EQ(settings.voices.voicesPerSound, defaults.voices.voicesPerSound);
    EXPECT_EQ(settings.voices.maxActiveVoices, defaults.voices.maxActiveVoices);
    EXPECT_FLOAT_EQ(settings.sfxVolume, defaults.sfxVolume);
    EXPECT_TRUE(settings.spatial.enabled);
    EXPECT_FLOAT_EQ(settings.spatial.maxPan, defaults.spatial.maxPan);
}

TEST(AudioSettings, ReadsEverySetting) {
    std::vector<std::string> warnings;
    const audio::AudioSettings settings = read(R"(
        [audio]
        backend = "null"
        voices_per_sound = 2
        max_active_voices = 16
        [audio.volume]
        master = 0.5
        music = 0.25
        sfx = 0.75
        [audio.spatial]
        enabled = false
        pan_range = 320.0
        max_pan = 1.0
        min_distance = 100
        max_distance = 400
    )",
                                               warnings);

    EXPECT_TRUE(warnings.empty()) << warnings.front();
    EXPECT_EQ(settings.backend, audio::AudioBackendKind::Null);
    EXPECT_EQ(settings.voices.voicesPerSound, 2U);
    EXPECT_EQ(settings.voices.maxActiveVoices, 16U);
    EXPECT_FLOAT_EQ(settings.masterVolume, 0.5F);
    EXPECT_FLOAT_EQ(settings.musicVolume, 0.25F);
    EXPECT_FLOAT_EQ(settings.sfxVolume, 0.75F);
    EXPECT_FALSE(settings.spatial.enabled);
    EXPECT_FLOAT_EQ(settings.spatial.panRange, 320.0F);
    EXPECT_FLOAT_EQ(settings.spatial.maxPan, 1.0F);
    EXPECT_FLOAT_EQ(settings.spatial.minDistance, 100.0F);
    EXPECT_FLOAT_EQ(settings.spatial.maxDistance, 400.0F);
}

TEST(AudioSettings, OutOfRangeValuesAreClampedAndReported) {
    std::vector<std::string> warnings;
    const audio::AudioSettings settings = read(R"(
        [audio]
        voices_per_sound = 1000000
        max_active_voices = 0
        [audio.volume]
        master = 3.0
        [audio.spatial]
        max_pan = -1.0
    )",
                                               warnings);

    EXPECT_EQ(settings.voices.voicesPerSound, audio::VOICES_PER_SOUND_LIMIT);
    EXPECT_EQ(settings.voices.maxActiveVoices, 1U);
    EXPECT_FLOAT_EQ(settings.masterVolume, audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(settings.spatial.maxPan, audio::PAN_CENTER);
    EXPECT_EQ(warnings.size(), 4U);
    EXPECT_TRUE(mentions(warnings, "audio.voices_per_sound"));
    EXPECT_TRUE(mentions(warnings, "audio.volume.master"));
}

TEST(AudioSettings, WrongTypesKeepTheDefaultAndAreReported) {
    std::vector<std::string> warnings;
    const audio::AudioSettings settings = read(R"(
        [audio]
        backend = 3
        voices_per_sound = 2.5
        [audio.volume]
        sfx = "loud"
        [audio.spatial]
        enabled = "yes"
    )",
                                               warnings);
    const audio::AudioSettings defaults;

    EXPECT_EQ(settings.backend, defaults.backend);
    EXPECT_EQ(settings.voices.voicesPerSound, defaults.voices.voicesPerSound);
    EXPECT_FLOAT_EQ(settings.sfxVolume, defaults.sfxVolume);
    EXPECT_EQ(settings.spatial.enabled, defaults.spatial.enabled);
    EXPECT_EQ(warnings.size(), 4U);
}

TEST(AudioSettings, NonFiniteNumbersAreRejected) {
    std::vector<std::string> warnings;
    const audio::AudioSettings settings = read("[audio.volume]\nmusic = nan\nsfx = inf\n", warnings);

    EXPECT_FLOAT_EQ(settings.musicVolume, audio::DEFAULT_VOLUME);
    EXPECT_FLOAT_EQ(settings.sfxVolume, audio::DEFAULT_VOLUME);
    EXPECT_EQ(warnings.size(), 2U);
}

TEST(AudioSettings, UnknownBackendFallsBackToAuto) {
    std::vector<std::string> warnings;
    const audio::AudioSettings settings = read("[audio]\nbackend = \"fmod\"\n", warnings);

    EXPECT_EQ(settings.backend, audio::AudioBackendKind::Auto);
    EXPECT_TRUE(mentions(warnings, "audio.backend"));
}

TEST(AudioSettings, MaxDistanceBelowMinDistanceIsRaised) {
    std::vector<std::string> warnings;
    const audio::AudioSettings settings =
        read("[audio.spatial]\nmin_distance = 500.0\nmax_distance = 100.0\n", warnings);

    EXPECT_FLOAT_EQ(settings.spatial.maxDistance, 500.0F);
    EXPECT_TRUE(mentions(warnings, "audio.spatial.max_distance"));
}

TEST(AudioSettings, UnknownKeysAreReported) {
    std::vector<std::string> warnings;
    (void)read("[audio]\nvoulme = 1.0\n[audio.volume]\nmastr = 0.5\n[render]\nvsync = true\n", warnings);

    EXPECT_EQ(warnings.size(), 2U);
    EXPECT_TRUE(mentions(warnings, "audio.voulme"));
    EXPECT_TRUE(mentions(warnings, "audio.volume.mastr"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
