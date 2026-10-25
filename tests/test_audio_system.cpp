#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "audio/IAudioBackend.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "audio/spatial/AudioSpatial.hpp"
#include "audio/spatial/SpatialConfig.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"
#include "components/AudioEmitter.hpp"
#include "components/AudioListener.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
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

    audio::VoiceId playSound(audio::SoundId id, const audio::SoundParams& params) override {
        if (_playing.size() >= _voiceLimit) {
            // Like the real backend: the oldest instance loses its voice.
            _stolen.push_back(_playing.front());
            _playing.erase(_playing.begin());
        }
        _sounds.push_back({.id = id, .params = params});
        _calls.emplace_back("playSound");
        _playing.push_back(++_lastVoice);
        return _lastVoice;
    }
    audio::VoiceId playSoundWithoutStealing(audio::SoundId id, const audio::SoundParams& params) override {
        return _playing.size() < _voiceLimit ? playSound(id, params) : audio::NO_VOICE;
    }
    audio::VoiceState updateVoice(audio::VoiceId voice, const audio::SoundParams& params) override {
        if (std::ranges::find(_stolen, voice) != _stolen.end()) {
            return audio::VoiceState::Stolen;
        }
        if (std::ranges::find(_playing, voice) == _playing.end()) {
            return audio::VoiceState::Ended;
        }
        _lastUpdate = params;
        return audio::VoiceState::Playing;
    }
    void stopVoice(audio::VoiceId voice) override {
        if (std::erase(_playing, voice) > 0) {
            _calls.emplace_back("stopVoice");
        }
    }
    void stopAllSounds() override {
        _calls.emplace_back("stopAllSounds");
        _playing.clear();
    }
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
    [[nodiscard]] std::size_t playingCount() const { return _playing.size(); }
    [[nodiscard]] const audio::SoundParams& lastUpdate() const { return _lastUpdate; }

    /** @brief Limits how many instances can play at once; beyond it, the oldest one is stolen. */
    void setVoiceLimit(std::size_t limit) { _voiceLimit = limit; }

    /** @brief Simulates every playing instance reaching its end. */
    void endAllVoices() { _playing.clear(); }

  private:
    std::vector<PlayedSound> _sounds;
    std::vector<std::string> _calls;
    float _masterVolume = -1.0F;
    float _musicVolume = -1.0F;
    int _updates = 0;
    std::vector<audio::VoiceId> _playing;
    std::vector<audio::VoiceId> _stolen;
    std::size_t _voiceLimit = std::numeric_limits<std::size_t>::max();
    audio::VoiceId _lastVoice = audio::NO_VOICE;
    audio::SoundParams _lastUpdate;
};

constexpr audio::SoundId LASER = 1;
constexpr audio::SoundId EXPLOSION = 2;
constexpr audio::MusicId THEME = 1;

/** @brief Audio system wired to a recording backend, with an empty world. */
class AudioSystemTest : public ::testing::Test {
  protected:
    /** @brief Adds a listener entity at the given world position. */
    ecs::Entity spawnListener(float x, float y, bool active = true) {
        const ecs::Entity entity = registry.spawnEntity();
        registry.addComponent<ecs::Transform>(entity, ecs::Transform{.x = x, .y = y});
        registry.addComponent<ecs::AudioListener>(entity, ecs::AudioListener{.active = active});
        return entity;
    }

    /** @brief Adds an entity emitting `sound` at the given world position. */
    ecs::Entity spawnEmitter(float x, float y, audio::SoundId sound, bool loop = true) {
        const ecs::Entity entity = registry.spawnEntity();
        registry.addComponent<ecs::Transform>(entity, ecs::Transform{.x = x, .y = y});
        registry.addComponent<ecs::AudioEmitter>(entity, ecs::AudioEmitter{.sound = sound, .loop = loop});
        return entity;
    }

    FakeAudioBackend backend;
    ecs::EventBus bus;
    ecs::Registry registry;
    systems::AudioSystem audioSystem{backend, bus};
};

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Spatial computations
// ---------------------------------------------------------------------------------------------------------------------

TEST(AudioSpatial, PanFollowsHorizontalOffset) {
    const audio::SpatialConfig config;

    EXPECT_FLOAT_EQ(audio::computePan(0.0F, config), audio::PAN_CENTER);
    EXPECT_FLOAT_EQ(audio::computePan(config.panRange / 2.0F, config), config.maxPan / 2.0F);
    EXPECT_FLOAT_EQ(audio::computePan(-config.panRange, config), -config.maxPan);
    EXPECT_FLOAT_EQ(audio::computePan(config.panRange * 10.0F, config), config.maxPan) << "pan is capped";
}

TEST(AudioSpatial, FullStrengthPanReachesTheSpeakers) {
    const audio::SpatialConfig config{.maxPan = 1.0F};

    EXPECT_FLOAT_EQ(audio::computePan(config.panRange, config), audio::PAN_RIGHT);
    EXPECT_FLOAT_EQ(audio::computePan(-config.panRange, config), audio::PAN_LEFT);
}

TEST(AudioSpatial, AttenuationFallsLinearlyBetweenMinAndMaxDistance) {
    const audio::SpatialConfig config;
    const float middle = (config.minDistance + config.maxDistance) / 2.0F;

    EXPECT_FLOAT_EQ(audio::computeAttenuation(0.0F, config), audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(audio::computeAttenuation(config.minDistance, config), audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(audio::computeAttenuation(middle, config), 0.5F);
    EXPECT_FLOAT_EQ(audio::computeAttenuation(config.maxDistance, config), audio::MIN_VOLUME);
    EXPECT_FLOAT_EQ(audio::computeAttenuation(config.maxDistance * 2.0F, config), audio::MIN_VOLUME);
}

TEST(AudioSpatial, VerticalOffsetAttenuatesWithoutPanning) {
    const audio::SpatialConfig config{.minDistance = 100.0F, .maxDistance = 300.0F};

    const audio::SpatialGains gains = audio::computeSpatial({.x = 0.0F, .y = 200.0F}, {.x = 0.0F, .y = 0.0F}, config);

    EXPECT_FLOAT_EQ(gains.pan, audio::PAN_CENTER);
    EXPECT_FLOAT_EQ(gains.volume, 0.5F);
}

TEST(AudioSpatial, InvalidInputsStayFinite) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const audio::SpatialConfig config;

    const audio::SpatialGains gains = audio::computeSpatial({.x = nan, .y = 0.0F}, {.x = 0.0F, .y = 0.0F}, config);
    EXPECT_FLOAT_EQ(gains.volume, audio::MAX_VOLUME) << "a broken position is heard like an unpositioned sound";
    EXPECT_FLOAT_EQ(gains.pan, audio::PAN_CENTER);

    EXPECT_FLOAT_EQ(audio::computePan(100.0F, audio::SpatialConfig{.panRange = 0.0F}), audio::PAN_CENTER);
    EXPECT_FLOAT_EQ(audio::computePan(100.0F, audio::SpatialConfig{.maxPan = nan}),
                    100.0F / audio::DEFAULT_PAN_RANGE * audio::DEFAULT_MAX_PAN);
    EXPECT_FLOAT_EQ(audio::computeAttenuation(500.0F, audio::SpatialConfig{.minDistance = nan}), audio::MAX_VOLUME);

    const audio::SpatialConfig collapsed{.minDistance = 100.0F, .maxDistance = 100.0F};
    EXPECT_FLOAT_EQ(audio::computeAttenuation(100.0F, collapsed), audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(audio::computeAttenuation(101.0F, collapsed), audio::MIN_VOLUME);
}

// ---------------------------------------------------------------------------------------------------------------------
// Event handling
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(AudioSystemTest, PublishedSoundIsPlayedOnUpdate) {
    bus.publish(events::PlaySoundEvent{.sound = LASER, .volume = 0.8F, .pitch = 1.5F});
    EXPECT_TRUE(backend.sounds().empty()) << "publishing must only queue the sound";

    audioSystem.update(registry);
    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_EQ(backend.sounds()[0].id, LASER);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.8F);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pitch, 1.5F);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, audio::PAN_CENTER);
}

TEST_F(AudioSystemTest, QueueIsEmptiedByUpdate) {
    bus.publish(events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);
    audioSystem.update(registry);

    EXPECT_EQ(backend.sounds().size(), 1U);
    EXPECT_EQ(backend.updates(), 2);
}

TEST_F(AudioSystemTest, SfxVolumeScalesSounds) {
    audioSystem.setVolume(audio::AudioBus::Sfx, 0.5F);
    bus.publish(events::PlaySoundEvent{.sound = LASER, .volume = 0.8F});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.4F);
}

TEST_F(AudioSystemTest, MutedSfxBusPlaysNothing) {
    audioSystem.setVolume(audio::AudioBus::Sfx, 0.0F);
    bus.publish(events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);

    EXPECT_TRUE(backend.sounds().empty());
}

TEST_F(AudioSystemTest, VolumeEventsReachTheBackend) {
    bus.publish(events::SetAudioVolumeEvent{.bus = audio::AudioBus::Master, .volume = 0.3F});
    bus.publish(events::SetAudioVolumeEvent{.bus = audio::AudioBus::Music, .volume = 0.6F});
    bus.publish(events::SetAudioVolumeEvent{.bus = audio::AudioBus::Sfx, .volume = 0.9F});
    audioSystem.update(registry);

    EXPECT_FLOAT_EQ(backend.masterVolume(), 0.3F);
    EXPECT_FLOAT_EQ(backend.musicVolume(), 0.6F);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Master), 0.3F);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Music), 0.6F);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Sfx), 0.9F);
}

TEST_F(AudioSystemTest, InvalidVolumesAreClamped) {
    audioSystem.setVolume(audio::AudioBus::Master, std::numeric_limits<float>::quiet_NaN());
    audioSystem.setVolume(audio::AudioBus::Music, -1.0F);
    audioSystem.setVolume(audio::AudioBus::Sfx, 3.0F);

    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Master), audio::DEFAULT_VOLUME);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Music), audio::MIN_VOLUME);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Sfx), audio::MAX_VOLUME);
}

TEST_F(AudioSystemTest, EventsAreExecutedInArrivalOrderAndSoundsLast) {
    bus.publish(events::PlayMusicEvent{.music = THEME, .loop = false});
    bus.publish(events::PlaySoundEvent{.sound = EXPLOSION});
    bus.publish(events::StopMusicEvent{});
    audioSystem.update(registry);

    const std::vector<std::string> expected{"playMusic 1 once", "stopMusic", "playSound"};
    EXPECT_EQ(backend.calls(), expected);
}

TEST_F(AudioSystemTest, StopAllSoundsDropsEarlierRequestsOfTheSameUpdate) {
    bus.publish(events::PlaySoundEvent{.sound = LASER});
    bus.publish(events::StopAllSoundsEvent{});
    bus.publish(events::PlaySoundEvent{.sound = EXPLOSION});
    audioSystem.update(registry);

    const std::vector<std::string> expected{"stopAllSounds", "playSound"};
    EXPECT_EQ(backend.calls(), expected);
    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_EQ(backend.sounds()[0].id, EXPLOSION);
}

TEST_F(AudioSystemTest, SameSoundInOneUpdatePlaysOnce) {
    bus.publish(events::PlaySoundEvent{.sound = LASER});
    bus.publish(events::PlaySoundEvent{.sound = LASER});
    bus.publish(events::PlaySoundEvent{.sound = EXPLOSION});
    bus.publish(events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 2U);

    bus.publish(events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 3U);
}

TEST_F(AudioSystemTest, InvalidHandlesAreIgnored) {
    bus.publish(events::PlaySoundEvent{});
    bus.publish(events::PlayMusicEvent{});
    audioSystem.update(registry);

    EXPECT_TRUE(backend.calls().empty());
}

TEST_F(AudioSystemTest, SoundQueueIsBounded) {
    for (std::size_t i = 1; i <= audio::MAX_QUEUED_SOUNDS + 50; ++i) {
        bus.publish(events::PlaySoundEvent{.sound = static_cast<audio::SoundId>(i)});
    }
    bus.publish(events::PlayMusicEvent{.music = THEME});
    audioSystem.update(registry);

    EXPECT_EQ(backend.sounds().size(), audio::MAX_QUEUED_SOUNDS);
    EXPECT_EQ(backend.calls().front(), "playMusic 1 loop") << "non-sound events are never dropped";
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
    ecs::Registry registry;
    systems::AudioSystem audioSystem(backend, bus);

    bus.publish(events::PlaySoundEvent{.sound = LASER, .position = audio::SoundPosition{.x = 10.0F, .y = 0.0F}});
    bus.publish(events::PlayMusicEvent{.music = THEME});
    bus.publish(events::SetAudioVolumeEvent{.bus = audio::AudioBus::Sfx, .volume = 0.5F});
    EXPECT_NO_THROW(audioSystem.update(registry));
}

// ---------------------------------------------------------------------------------------------------------------------
// Positioned sounds
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(AudioSystemTest, PositionedSoundIsPannedTowardsItsSide) {
    spawnListener(640.0F, 360.0F);
    const audio::SpatialConfig& config = audioSystem.spatialConfig();

    bus.publish(events::PlaySoundEvent{.sound = LASER, .position = audio::SoundPosition{.x = 960.0F, .y = 360.0F}});
    bus.publish(events::PlaySoundEvent{.sound = EXPLOSION, .position = audio::SoundPosition{.x = 0.0F, .y = 360.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 2U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, 320.0F / config.panRange * config.maxPan);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, audio::MAX_VOLUME) << "visible sounds keep their volume";
    EXPECT_FLOAT_EQ(backend.sounds()[1].params.pan, -config.maxPan);
}

TEST_F(AudioSystemTest, DistantSoundsFadeAndSilentOnesAreSkipped) {
    spawnListener(0.0F, 0.0F);
    audioSystem.setSpatialConfig({.minDistance = 100.0F, .maxDistance = 300.0F});

    bus.publish(events::PlaySoundEvent{.sound = LASER, .volume = 0.8F, .position = audio::SoundPosition{.x = 200.0F}});
    bus.publish(events::PlaySoundEvent{.sound = EXPLOSION, .position = audio::SoundPosition{.x = 400.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U) << "a silent sound must not take a voice";
    EXPECT_EQ(backend.sounds()[0].id, LASER);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.4F);
}

TEST_F(AudioSystemTest, LoudestRequestOfTheSameSoundWins) {
    spawnListener(0.0F, 0.0F);
    audioSystem.setSpatialConfig({.minDistance = 100.0F, .maxDistance = 300.0F});

    bus.publish(events::PlaySoundEvent{.sound = LASER, .position = audio::SoundPosition{.x = 250.0F}});
    bus.publish(events::PlaySoundEvent{.sound = LASER, .position = audio::SoundPosition{.x = -150.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, 0.75F);
    EXPECT_LT(backend.sounds()[0].params.pan, audio::PAN_CENTER) << "the closer emitter, on the left, is kept";
}

TEST_F(AudioSystemTest, SoundsAreNotSpatializedWithoutListener) {
    bus.publish(events::PlaySoundEvent{.sound = LASER, .position = audio::SoundPosition{.x = 5000.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, audio::PAN_CENTER);
}

TEST_F(AudioSystemTest, InactiveListenerIsIgnored) {
    spawnListener(5000.0F, 0.0F, false);
    spawnListener(0.0F, 0.0F);

    bus.publish(events::PlaySoundEvent{.sound = LASER, .position = audio::SoundPosition{.x = 100.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, audio::MAX_VOLUME);
    EXPECT_GT(backend.sounds()[0].params.pan, audio::PAN_CENTER);
}

TEST_F(AudioSystemTest, DisabledSpatializationPlaysPositionedSoundsCentered) {
    spawnListener(0.0F, 0.0F);
    audioSystem.setSpatialConfig({.enabled = false});

    bus.publish(events::PlaySoundEvent{.sound = LASER, .position = audio::SoundPosition{.x = 5000.0F}});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U) << "a far sound is not skipped when spatialization is off";
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, audio::PAN_CENTER);
}

TEST_F(AudioSystemTest, SettingsApplyVolumesAndSpatialization) {
    audioSystem.applySettings(audio::AudioSettings{
        .spatial = {.enabled = false}, .masterVolume = 0.2F, .musicVolume = 0.4F, .sfxVolume = 0.6F});

    EXPECT_FLOAT_EQ(backend.masterVolume(), 0.2F);
    EXPECT_FLOAT_EQ(backend.musicVolume(), 0.4F);
    EXPECT_FLOAT_EQ(audioSystem.volume(audio::AudioBus::Sfx), 0.6F);
    EXPECT_FALSE(audioSystem.spatialConfig().enabled);
}

TEST_F(AudioSystemTest, UnpositionedSoundIgnoresTheListener) {
    spawnListener(5000.0F, 0.0F);

    bus.publish(events::PlaySoundEvent{.sound = LASER});
    audioSystem.update(registry);

    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.volume, audio::MAX_VOLUME);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, audio::PAN_CENTER);
}

// ---------------------------------------------------------------------------------------------------------------------
// Emitters
// ---------------------------------------------------------------------------------------------------------------------

TEST_F(AudioSystemTest, EmitterSoundFollowsItsEntity) {
    spawnListener(640.0F, 360.0F);
    const ecs::Entity ship = spawnEmitter(0.0F, 360.0F, LASER);
    const audio::SpatialConfig& config = audioSystem.spatialConfig();

    audioSystem.update(registry);
    ASSERT_EQ(backend.sounds().size(), 1U);
    EXPECT_FLOAT_EQ(backend.sounds()[0].params.pan, -config.maxPan);

    registry.getComponents<ecs::Transform>().get(ship).x = 1280.0F;
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 1U) << "the playing instance is moved, not restarted";
    EXPECT_FLOAT_EQ(backend.lastUpdate().pan, config.maxPan);
}

TEST_F(AudioSystemTest, LoopingEmitterRestartsWhenItsSoundEnds) {
    spawnEmitter(0.0F, 0.0F, LASER);

    audioSystem.update(registry);
    backend.endAllVoices();
    audioSystem.update(registry);

    EXPECT_EQ(backend.sounds().size(), 2U);
}

TEST_F(AudioSystemTest, NonLoopingEmitterPlaysOnce) {
    spawnEmitter(0.0F, 0.0F, LASER, false);

    audioSystem.update(registry);
    backend.endAllVoices();
    audioSystem.update(registry);

    EXPECT_EQ(backend.sounds().size(), 1U);
}

TEST_F(AudioSystemTest, EmitterStopsWithItsEntity) {
    const ecs::Entity ship = spawnEmitter(0.0F, 0.0F, LASER);
    audioSystem.update(registry);
    ASSERT_EQ(backend.playingCount(), 1U);

    registry.killEntity(ship);
    audioSystem.update(registry);

    EXPECT_EQ(backend.playingCount(), 0U);
    EXPECT_EQ(backend.calls().back(), "stopVoice");
}

TEST_F(AudioSystemTest, EmitterCanBePausedAndResumed) {
    const ecs::Entity ship = spawnEmitter(0.0F, 0.0F, LASER, false);
    audioSystem.update(registry);

    registry.getComponents<ecs::AudioEmitter>().get(ship).playing = false;
    audioSystem.update(registry);
    EXPECT_EQ(backend.playingCount(), 0U);

    registry.getComponents<ecs::AudioEmitter>().get(ship).playing = true;
    audioSystem.update(registry);
    EXPECT_EQ(backend.playingCount(), 1U);
    EXPECT_EQ(backend.sounds().size(), 2U);
}

TEST_F(AudioSystemTest, InaudibleEmitterStartsWhenItComesCloser) {
    spawnListener(0.0F, 0.0F);
    audioSystem.setSpatialConfig({.minDistance = 100.0F, .maxDistance = 300.0F});
    const ecs::Entity ship = spawnEmitter(1000.0F, 0.0F, LASER);

    audioSystem.update(registry);
    EXPECT_TRUE(backend.sounds().empty());

    registry.getComponents<ecs::Transform>().get(ship).x = 50.0F;
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 1U);
}

TEST_F(AudioSystemTest, LoopingEmittersDoNotStealEachOtherBack) {
    backend.setVoiceLimit(4);
    std::vector<ecs::Entity> ships;
    for (int i = 0; i < 5; ++i) {
        ships.push_back(spawnEmitter(0.0F, 0.0F, LASER));
    }

    audioSystem.update(registry);
    ASSERT_EQ(backend.sounds().size(), 5U) << "the fifth emitter took the voice of the first";

    for (int frame = 0; frame < 10; ++frame) {
        audioSystem.update(registry);
    }
    EXPECT_EQ(backend.sounds().size(), 5U) << "no emitter may restart while every voice is in use";
    EXPECT_EQ(backend.playingCount(), 4U);

    // The destroyed emitter frees its voice at the end of this update; the stolen one takes it on the next.
    registry.killEntity(ships.back());
    audioSystem.update(registry);
    audioSystem.update(registry);
    EXPECT_EQ(backend.sounds().size(), 6U) << "the stolen emitter resumes once a voice is free";
    EXPECT_EQ(backend.playingCount(), 4U);
}

TEST(AudioSystem, StopsEmittersWhenDestroyed) {
    FakeAudioBackend backend;
    ecs::EventBus bus;
    ecs::Registry registry;
    const ecs::Entity ship = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(ship, ecs::Transform{});
    registry.addComponent<ecs::AudioEmitter>(ship, ecs::AudioEmitter{.sound = LASER});
    {
        systems::AudioSystem audioSystem(backend, bus);
        audioSystem.update(registry);
        ASSERT_EQ(backend.playingCount(), 1U);
    }
    EXPECT_EQ(backend.playingCount(), 0U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
