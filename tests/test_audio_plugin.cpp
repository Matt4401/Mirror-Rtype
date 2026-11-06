#include <gtest/gtest.h>

#include <cstddef>
#include <memory>

#include "audio/AudioPlugin.hpp"
#include "audio/NullAudioBackend.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "audio/types/AudioTypes.hpp"
#include "core/Engine.hpp"
#include "core/services/IAudioService.hpp"

namespace {

/** @brief Settings with distinct bus volumes, so the tests can check they were applied. */
audio::AudioSettings customSettings() {
    audio::AudioSettings settings;
    settings.masterVolume = 0.5F;
    settings.musicVolume = 0.25F;
    settings.sfxVolume = 0.75F;
    return settings;
}

}  // namespace

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
