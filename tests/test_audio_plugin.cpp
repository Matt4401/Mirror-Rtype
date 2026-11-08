#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPlugin.hpp"
#include "audio/AudioPlugin.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"
#include "core/Engine.hpp"
#include "core/services/IAudioService.hpp"
#include "ecs/Registry.hpp"

namespace {

/** @brief Settings with distinct bus volumes, so the tests can check they were applied. */
audio::AudioSettings customSettings() {
    audio::AudioSettings settings;
    settings.masterVolume = 0.5F;
    settings.musicVolume = 0.25F;
    settings.sfxVolume = 0.75F;
    return settings;
}

/** @brief Muted backend that hands out handles and records what it loads and releases. */
class RecordingBackend final : public audio::IAudioBackend {
  public:
    /** @brief What the backend did, shared with the test after the plugin owns the backend. */
    struct Log {
        std::vector<std::string> loads;
        std::vector<audio::SoundId> unloadedSounds;
        std::vector<audio::MusicId> unloadedMusics;
    };

    explicit RecordingBackend(std::shared_ptr<Log> log) : _log(std::move(log)) {}

    [[nodiscard]] bool isAvailable() const noexcept override { return false; }
    [[nodiscard]] audio::SoundId loadSound(const std::string& path) override {
        _log->loads.push_back(path);
        return static_cast<audio::SoundId>(_log->loads.size());
    }
    [[nodiscard]] audio::MusicId loadMusic(const std::string& path) override {
        _log->loads.push_back(path);
        return static_cast<audio::MusicId>(_log->loads.size());
    }
    void unloadSound(audio::SoundId id) override { _log->unloadedSounds.push_back(id); }
    void unloadMusic(audio::MusicId id) override { _log->unloadedMusics.push_back(id); }
    audio::VoiceId playSound(audio::SoundId /*id*/, const audio::SoundParams& /*params*/) override {
        return audio::NO_VOICE;
    }
    audio::VoiceId playSoundWithoutStealing(audio::SoundId /*id*/, const audio::SoundParams& /*params*/) override {
        return audio::NO_VOICE;
    }
    audio::VoiceState updateVoice(audio::VoiceId /*voice*/, const audio::SoundParams& /*params*/) override {
        return audio::VoiceState::Ended;
    }
    void stopVoice(audio::VoiceId /*voice*/) override {}
    void stopAllSounds() override {}
    void playMusic(audio::MusicId /*id*/, bool /*loop*/) override {}
    void stopMusic() override {}
    void setMusicVolume(float /*volume*/) override {}
    void setMasterVolume(float /*volume*/) override {}
    void update() override {}
    [[nodiscard]] std::size_t activeVoiceCount() const override { return 0; }

  private:
    std::shared_ptr<Log> _log;
};

}  // namespace

TEST(AudioPlugin, PlugsItsLoadersIntoTheAssetManagerAndReleasesThemAtShutdown) {
    auto log = std::make_shared<RecordingBackend::Log>();
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<RecordingBackend>(log)).addPlugin<assets::AssetPlugin>();
    engine.init();

    auto& manager = engine.registry().getResource<assets::AssetManager>();
    ASSERT_TRUE(manager.hasLoader<assets::Sound>());
    ASSERT_TRUE(manager.hasLoader<assets::Music>());
    const audio::SoundId jump = manager.load<assets::Sound>("sounds/jump.wav");
    EXPECT_EQ(manager.load<assets::Sound>("sounds/jump.wav"), jump);
    const audio::MusicId theme = manager.load<assets::Music>("music/theme.ogg");
    EXPECT_EQ(log->loads, (std::vector<std::string>{"sounds/jump.wav", "music/theme.ogg"}));

    engine.shutdown();
    EXPECT_EQ(log->unloadedSounds, std::vector<audio::SoundId>{jump});
    EXPECT_EQ(log->unloadedMusics, std::vector<audio::MusicId>{theme});
}

TEST(AudioPlugin, RunsWithoutAnAssetManager) {
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<audio::NullAudioBackend>());
    engine.init();
    EXPECT_FALSE(engine.registry().hasResource<assets::AssetManager>());
    engine.shutdown();
}

TEST(AudioPlugin, RegistersTheAudioServiceAtInit) {
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<audio::NullAudioBackend>());

    EXPECT_FALSE(engine.mediator().hasService<engine::IAudioService>());
    engine.init();

    EXPECT_TRUE(engine.mediator().hasService<engine::IAudioService>());
    EXPECT_NE(engine.getPlugin<audio::AudioPlugin>().audioSystem(), nullptr);
}

TEST(AudioPlugin, AppliesSettingsAtInit) {
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<audio::NullAudioBackend>(), customSettings());
    engine.init();

    const auto& service = engine.service<engine::IAudioService>();
    EXPECT_FLOAT_EQ(service.volume(audio::AudioBus::Master), 0.5F);
    EXPECT_FLOAT_EQ(service.volume(audio::AudioBus::Music), 0.25F);
    EXPECT_FLOAT_EQ(service.volume(audio::AudioBus::Sfx), 0.75F);
}

TEST(AudioPlugin, ServiceReportsAMutedBackend) {
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<audio::NullAudioBackend>());
    engine.init();

    auto& service = engine.service<engine::IAudioService>();
    EXPECT_FALSE(service.isAvailable());
    EXPECT_EQ(service.loadSound("missing.wav"), audio::NO_SOUND);
    EXPECT_EQ(service.loadMusic("missing.ogg"), audio::NO_MUSIC);
    EXPECT_EQ(service.activeVoiceCount(), 0U);
}

TEST(AudioPlugin, NullBackendIsReplacedByAMutedOne) {
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::unique_ptr<audio::IAudioBackend>{});

    EXPECT_NE(engine.getPlugin<audio::AudioPlugin>().backend(), nullptr);
    EXPECT_NO_THROW(engine.init());
}

TEST(AudioPlugin, StepsWithoutThrowing) {
    double now = 0.0;
    engine::Engine engine([&now]() -> double { return now; });
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<audio::NullAudioBackend>());
    engine.init();

    now = static_cast<double>(engine.fixedDeltaTime()) * 1.5;
    std::size_t steps = 0;
    EXPECT_NO_THROW(steps = engine.advance());
    EXPECT_EQ(steps, 1U);
}

TEST(AudioPlugin, ShutdownUnregistersTheServiceAndKeepsTheInjectedBackend) {
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<audio::NullAudioBackend>());
    engine.init();
    engine.shutdown();

    auto& plugin = engine.getPlugin<audio::AudioPlugin>();
    EXPECT_FALSE(engine.mediator().hasService<engine::IAudioService>());
    EXPECT_EQ(plugin.audioSystem(), nullptr);
    EXPECT_NE(plugin.backend(), nullptr);
}

TEST(AudioPlugin, CanBeInitializedAgainAfterShutdown) {
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<audio::NullAudioBackend>());
    engine.init();
    engine.shutdown();

    // An engine does not restart after shutdown(), but the plugin itself can be initialized again.
    auto& plugin = engine.getPlugin<audio::AudioPlugin>();
    EXPECT_NO_THROW(plugin.init(engine));
    EXPECT_TRUE(engine.mediator().hasService<engine::IAudioService>());
    plugin.shutdown(engine);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
