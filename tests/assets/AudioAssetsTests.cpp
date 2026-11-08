#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPlugin.hpp"
#include "assets/audio/AudioAssetsPlugin.hpp"
#include "audio/AudioPlugin.hpp"
#include "audio/IAudioBackend.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "audio/types/SoundParams.hpp"
#include "core/Engine.hpp"
#include "ecs/Registry.hpp"

namespace {

/** @brief Backend that hands out handles (giving `preferred` back) and records what it loads and releases. */
class RecordingBackend final : public audio::IAudioBackend {
  public:
    /** @brief What the backend did, shared with the test after the plugin owns the backend. */
    struct Log {
        std::vector<std::string> loads;
        std::vector<audio::SoundId> unloadedSounds;
        std::vector<audio::MusicId> unloadedMusics;
        std::vector<audio::SoundId> preferredSounds;
    };

    explicit RecordingBackend(std::shared_ptr<Log> log) : _log(std::move(log)) {}

    [[nodiscard]] bool isAvailable() const noexcept override { return true; }
    [[nodiscard]] audio::SoundId loadSound(const std::string& path) override { return loadSoundAs(path, 0); }
    [[nodiscard]] audio::MusicId loadMusic(const std::string& path) override {
        _log->loads.push_back(path);
        return static_cast<audio::MusicId>(_log->loads.size());
    }
    [[nodiscard]] audio::SoundId loadSoundAs(const std::string& path, audio::SoundId preferred) override {
        _log->loads.push_back(path);
        _log->preferredSounds.push_back(preferred);
        return preferred != audio::NO_SOUND ? preferred : static_cast<audio::SoundId>(_log->loads.size());
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

TEST(AudioAssetsPlugin, PlugsTheAudioLoadersIntoTheAssetManagerAndReleasesThemAtShutdown) {
    auto log = std::make_shared<RecordingBackend::Log>();
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<RecordingBackend>(log))
        .addPlugin<assets::AssetPlugin>()
        .addPlugin<assets::AudioAssetsPlugin>();
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

TEST(AudioAssetsPlugin, ASoundLoadedAgainKeepsItsHandle) {
    auto log = std::make_shared<RecordingBackend::Log>();
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<RecordingBackend>(log))
        .addPlugin<assets::AssetPlugin>()
        .addPlugin<assets::AudioAssetsPlugin>();
    engine.init();

    auto& manager = engine.registry().getResource<assets::AssetManager>();
    manager.declare<assets::Sound>("boom", "sounds/boom.wav", "level1");
    manager.loadGroup("level1");
    const audio::SoundId boom = manager.get<assets::Sound>("boom");
    manager.unloadGroup("level1");
    manager.loadGroup("level1");
    EXPECT_EQ(manager.get<assets::Sound>("boom"), boom);
    EXPECT_EQ(log->preferredSounds.back(), boom);
    engine.shutdown();
}

TEST(AudioAssetsPlugin, AMutedBackendInstallsNoLoader) {
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<audio::NullAudioBackend>())
        .addPlugin<assets::AssetPlugin>()
        .addPlugin<assets::AudioAssetsPlugin>();
    engine.init();

    auto& manager = engine.registry().getResource<assets::AssetManager>();
    EXPECT_FALSE(engine.getPlugin<assets::AudioAssetsPlugin>().attached());
    EXPECT_FALSE(manager.hasLoader<assets::Sound>());
    manager.declare<assets::Sound>("jump", "sounds/jump.wav");
    manager.loadGroup("global");
    // Waiting for a loader, not failed: no warning per sound.
    EXPECT_EQ(manager.status<assets::Sound>("jump"), assets::AssetStatus::Declared);
    EXPECT_TRUE(manager.warnings().empty());
    engine.shutdown();
}

TEST(AudioAssetsPlugin, TheAudioPluginRunsWithoutTheAssetManager) {
    engine::Engine engine;
    engine.addPlugin<audio::AudioPlugin>(std::make_unique<audio::NullAudioBackend>());
    engine.init();
    EXPECT_FALSE(engine.registry().hasResource<assets::AssetManager>());
    engine.shutdown();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
