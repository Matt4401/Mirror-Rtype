#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "audio/IAudioBackend.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioTypes.hpp"
#include "audio/common/SoundParams.hpp"
#include "events/AudioEvents.hpp"
#include "events/EventBus.hpp"
#include "systems/audio/AudioSystem.hpp"

namespace {

/** @brief Backend recording every call, so the tests can check what the audio system asked for. */
class FakeAudioBackend final : public audio::IAudioBackend {
  public:
    struct PlayedSound {
        audio::SoundId id = audio::NO_SOUND;
        audio::SoundParams params;
    };

    [[nodiscard]] bool isAvailable() const noexcept override { return true; }
    [[nodiscard]] audio::SoundId loadSound(const std::string& /*path*/) override { return audio::NO_SOUND; }
    [[nodiscard]] audio::MusicId loadMusic(const std::string& /*path*/) override { return audio::NO_MUSIC; }

    bool playSound(audio::SoundId id, const audio::SoundParams& params) override {
        _sounds.push_back({.id = id, .params = params});
        _calls.emplace_back("playSound");
        return true;
    }
    void stopAllSounds() override { _calls.emplace_back("stopAllSounds"); }
    void playMusic(audio::MusicId id, bool loop) override {
        _calls.push_back("playMusic " + std::to_string(id) + (loop ? " loop" : " once"));
    }
    void stopMusic() override { _calls.emplace_back("stopMusic"); }
    void setMusicVolume(float volume) override { _musicVolume = volume; }
    void setMasterVolume(float volume) override { _masterVolume = volume; }
    void update() override { ++_updates; }
    [[nodiscard]] std::size_t activeVoiceCount() const override { return 0; }

    [[nodiscard]] const std::vector<PlayedSound>& sounds() const { return _sounds; }
    [[nodiscard]] const std::vector<std::string>& calls() const { return _calls; }
    [[nodiscard]] float masterVolume() const { return _masterVolume; }
    [[nodiscard]] float musicVolume() const { return _musicVolume; }
    [[nodiscard]] int updates() const { return _updates; }

  private:
    std::vector<PlayedSound> _sounds;
    std::vector<std::string> _calls;
    float _masterVolume = -1.0F;
    float _musicVolume = -1.0F;
    int _updates = 0;
};

constexpr audio::SoundId LASER = 1;
constexpr audio::SoundId EXPLOSION = 2;
constexpr audio::MusicId THEME = 1;

}  // namespace

TEST(AudioSystem, PublishedSoundIsPlayedOnUpdate) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    bus.publish(events::PlaySoundEvent{.sound = LASER, .volume = 0.8F, .pitch = 1.5F});
    EXPECT_TRUE(backend.sounds().empty()) << "publishing must only queue the sound";

    audioSystem.update();
    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_EQ(backend.sounds()[0].id, LASER);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.8F);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pitch, 1.5F);
}

TEST(AudioSystem, QueueIsEmptiedByUpdate) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    bus.publish(events::PlaySoundEvent{.sound = LASER});
    audioSystem.update();
    audioSystem.update();

    EXPECT_EQ(backend.sounds().size(), 1U);
    EXPECT_EQ(backend.updates(), 2);
}

TEST(AudioSystem, SfxVolumeScalesSounds) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    audioSystem.setVolume(audio::AudioBus::Sfx, 0.5F);
    bus.publish(events::PlaySoundEvent{.sound = LASER, .volume = 0.8F});
    audioSystem.update();

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.4F);
}

TEST(AudioSystem, VolumeEventsReachTheBackend) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    bus.publish(events::SetAudioVolumeEvent{.bus = audio::AudioBus::Master, .volume = 0.3F});
    bus.publish(events::SetAudioVolumeEvent{.bus = audio::AudioBus::Music, .volume = 0.6F});
    bus.publish(events::SetAudioVolumeEvent{.bus = audio::AudioBus::Sfx, .volume = 0.9F});
    audioSystem.update();

    EXPECT_FLOAT_EQ(backend.masterVolume(), 0.3F);
    EXPECT_FLOAT_EQ(backend.musicVolume(), 0.6F);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Master), 0.3F);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Music), 0.6F);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Sfx), 0.9F);
}

TEST(AudioSystem, InvalidVolumesAreClamped) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    audioSystem.setVolume(audio::AudioBus::Master, std::numeric_limits<float>::quiet_NaN());
    audioSystem.setVolume(audio::AudioBus::Music, -1.0F);
    audioSystem.setVolume(audio::AudioBus::Sfx, 3.0F);

    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Master), audio::DEFAULT_VOLUME);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Music), audio::MIN_VOLUME);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Sfx), audio::MAX_VOLUME);
}

TEST(AudioSystem, EventsAreExecutedInArrivalOrder) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    bus.publish(events::PlayMusicEvent{.music = THEME, .loop = false});
    bus.publish(events::PlaySoundEvent{.sound = LASER});
    bus.publish(events::StopAllSoundsEvent{});
    bus.publish(events::StopMusicEvent{});
    audioSystem.update();

    const std::vector<std::string> expected{"playMusic 1 once", "playSound", "stopAllSounds", "stopMusic"};
    EXPECT_EQ(backend.calls(), expected);
}

TEST(AudioSystem, SameSoundInOneUpdatePlaysOnce) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    bus.publish(events::PlaySoundEvent{.sound = LASER});
    bus.publish(events::PlaySoundEvent{.sound = LASER});
    bus.publish(events::PlaySoundEvent{.sound = EXPLOSION});
    bus.publish(events::PlaySoundEvent{.sound = LASER});
    audioSystem.update();
    EXPECT_EQ(backend.sounds().size(), 2U);

    bus.publish(events::PlaySoundEvent{.sound = LASER});
    audioSystem.update();
    EXPECT_EQ(backend.sounds().size(), 3U);
}

TEST(AudioSystem, InvalidHandlesAreIgnored) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    bus.publish(events::PlaySoundEvent{});
    bus.publish(events::PlayMusicEvent{});
    audioSystem.update();

    EXPECT_TRUE(backend.calls().empty());
}

TEST(AudioSystem, SoundQueueIsBounded) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    for (std::size_t i = 1; i <= audio::MAX_QUEUED_SOUNDS + 50; ++i) {
        bus.publish(events::PlaySoundEvent{.sound = static_cast<audio::SoundId>(i)});
    }
    bus.publish(events::PlayMusicEvent{.music = THEME});
    audioSystem.update();

    EXPECT_EQ(backend.sounds().size(), audio::MAX_QUEUED_SOUNDS);
    EXPECT_EQ(backend.calls().back(), "playMusic 1 loop") << "non-sound events are never dropped";
}

TEST(AudioSystem, StopsListeningWhenDestroyed) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    {
        const systems::AudioSystem audioSystem(backend, bus);
    }

    bus.publish(events::PlaySoundEvent{.sound = LASER});
    bus.publish(events::StopMusicEvent{});
    EXPECT_TRUE(backend.calls().empty());
}

TEST(AudioSystem, WorksWithAMutedBackend) {
    audio::NullAudioBackend backend;
    ecs::EventBus bus;
    systems::AudioSystem audioSystem(backend, bus);

    bus.publish(events::PlaySoundEvent{.sound = LASER});
    bus.publish(events::PlayMusicEvent{.music = THEME});
    bus.publish(events::SetAudioVolumeEvent{.bus = audio::AudioBus::Sfx, .volume = 0.5F});
    EXPECT_NO_THROW(audioSystem.update());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
