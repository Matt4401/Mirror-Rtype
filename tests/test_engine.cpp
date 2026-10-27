#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/plugins/TimePlugin.hpp"
#include "ecs/time/Time.hpp"

namespace {

class LifecycleRecorderPlugin final : public engine::IPlugin {
  public:
    explicit LifecycleRecorderPlugin(std::vector<std::string>& log) : _log(log) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "LifecycleRecorderPlugin"; }

    void build(engine::PluginBuilder& /*builder*/) override { _log.push_back("build"); }

    void init(engine::Engine& /*engine*/) override { _log.push_back("init"); }

    void fixedUpdate(engine::Engine& /*engine*/, const ecs::time::Time& /*time*/) override {
        _log.push_back("fixedUpdate");
    }

    void update(engine::Engine& /*engine*/, const ecs::time::Time& /*time*/) override { _log.push_back("update"); }

    void render(engine::Engine& /*engine*/) override { _log.push_back("render"); }

    void shutdown(engine::Engine& /*engine*/) override { _log.push_back("shutdown"); }

  private:
    std::vector<std::string>& _log;
};

class FrameTimeRecorderPlugin final : public engine::IPlugin {
  public:
    explicit FrameTimeRecorderPlugin(float& recordedDt) : _recordedDt(recordedDt) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "FrameTimeRecorderPlugin"; }

    void build(engine::PluginBuilder& /*builder*/) override {}

    void update(engine::Engine& engine, const ecs::time::Time& time) override {
        _recordedDt = time.deltaTime;
        engine.stop();
    }

  private:
    float& _recordedDt;
};

class FailingInitPlugin final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "FailingInitPlugin"; }
    void build(engine::PluginBuilder& /*builder*/) override {}
    void init(engine::Engine& /*engine*/) override { throw std::runtime_error("Simulated init failure"); }
};

}  // namespace

TEST(Engine, AutomaticallyLoadsImplicitPlugins) {
    engine::Engine engineInstance;
    EXPECT_FALSE(engineInstance.hasPlugin<engine::CorePlugin>());
    EXPECT_FALSE(engineInstance.hasPlugin<engine::TimePlugin>());

    engineInstance.init();

    EXPECT_TRUE(engineInstance.hasPlugin<engine::CorePlugin>());
    EXPECT_TRUE(engineInstance.hasPlugin<engine::TimePlugin>());
    EXPECT_TRUE(engineInstance.registry().hasResource<ecs::time::Time>());
}

TEST(Engine, ExecutesPhasesInSequence) {
    std::vector<std::string> log;
    {
        engine::Engine engineInstance;
        engineInstance.addPlugin<LifecycleRecorderPlugin>(log);

        EXPECT_TRUE(log.empty());

        engineInstance.step(0.016F);

        ASSERT_GE(log.size(), 5U);
        EXPECT_EQ(log[0], "build");
        EXPECT_EQ(log[1], "init");
        EXPECT_EQ(log[2], "fixedUpdate");
        EXPECT_EQ(log[3], "update");
        EXPECT_EQ(log[4], "render");

        engineInstance.shutdown();
        EXPECT_EQ(log.back(), "shutdown");
    }
}

TEST(Engine, RunPassesRealDeltaTimeToUpdate) {
    double now = 0.0;
    auto fakeSource = [&now]() -> double { return now; };

    float recordedDt = 0.0F;
    engine::Engine engineInstance(fakeSource);
    engineInstance.addPlugin<FrameTimeRecorderPlugin>(recordedDt);

    // Simulate a 144 Hz frame (approx 0.006944s) where dt < fixedDt
    now = 0.006944;

    engineInstance.run();

    EXPECT_NEAR(recordedDt, 0.006944F, 0.0001F);
}

TEST(Engine, InitFailureReleasesAlreadyInitializedPlugins) {
    std::vector<std::string> log;
    engine::Engine engineInstance;
    engineInstance.addPlugin<LifecycleRecorderPlugin>(log);
    engineInstance.addPlugin<FailingInitPlugin>();

    EXPECT_THROW(engineInstance.init(), std::runtime_error);

    ASSERT_GE(log.size(), 2U);
    EXPECT_EQ(log[0], "build");
    EXPECT_EQ(log[1], "init");
    EXPECT_EQ(log.back(), "shutdown");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
