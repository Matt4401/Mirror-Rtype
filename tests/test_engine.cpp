#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "core/plugins/TimePlugin.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"

namespace {

class LifecycleRecorderPlugin final : public engine::IPlugin {
  public:
    explicit LifecycleRecorderPlugin(std::vector<std::string>& log) : _log(log) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "LifecycleRecorderPlugin"; }

    void build(engine::PluginBuilder& /*builder*/) override { _log.push_back("build"); }

    void init(engine::Engine& /*engine*/) override { _log.push_back("init"); }

    void preUpdate(engine::Engine& /*engine*/) override { _log.push_back("preUpdate"); }

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

class IndependentUserPlugin final : public engine::IPlugin {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "IndependentUserPlugin"; }
};

/** @brief Number of times `phase` was recorded. */
std::size_t countOf(const std::vector<std::string>& log, const std::string_view phase) {
    return static_cast<std::size_t>(std::ranges::count(log, phase));
}

/** @brief Engine driven by a fake clock, with a fixed step of 0.25 s (exact in binary, so no rounding surprise). */
class EngineAdvanceTest : public ::testing::Test {
  protected:
    static constexpr float FIXED_DT = 0.25F;

    void SetUp() override {
        engine.setFixedDeltaTime(FIXED_DT);
        engine.addPlugin<LifecycleRecorderPlugin>(log);
    }

    double now = 0.0;
    std::vector<std::string> log;
    engine::Engine engine{[this]() -> double { return now; }};
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

        ASSERT_GE(log.size(), 6U);
        EXPECT_EQ(log[0], "build");
        EXPECT_EQ(log[1], "init");
        EXPECT_EQ(log[2], "preUpdate");
        EXPECT_EQ(log[3], "fixedUpdate");
        EXPECT_EQ(log[4], "update");
        EXPECT_EQ(log[5], "render");

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

TEST(Engine, StepAdvancesSimulationClockAccumulatingTicksAndElapsed) {
    engine::Engine engineInstance;
    const float fixedDt = engineInstance.fixedDeltaTime();

    engineInstance.step(0.016F);
    EXPECT_EQ(engineInstance.clock().time().tickCount, 1U);
    EXPECT_FLOAT_EQ(engineInstance.clock().time().totalElapsedTime, fixedDt);

    engineInstance.step(0.016F);
    EXPECT_EQ(engineInstance.clock().time().tickCount, 2U);
    EXPECT_FLOAT_EQ(engineInstance.clock().time().totalElapsedTime, 2.0F * fixedDt);
}

TEST(Engine, ImplicitPluginsOrderedBeforeIndependentUserPlugins) {
    engine::Engine engineInstance;
    engineInstance.addPlugin<IndependentUserPlugin>();

    engineInstance.init();

    const auto& order = engineInstance.executionOrder();
    ASSERT_GE(order.size(), 3U);
    EXPECT_EQ(order[0]->name(), "CorePlugin");
    EXPECT_EQ(order[1]->name(), "TimePlugin");
    EXPECT_EQ(order[2]->name(), "IndependentUserPlugin");
}

TEST_F(EngineAdvanceTest, InitializesTheEngine) {
    engine.advance();

    ASSERT_GE(log.size(), 2U);
    EXPECT_EQ(log[0], "build");
    EXPECT_EQ(log[1], "init");
}

TEST_F(EngineAdvanceTest, RunsNoStepBeforeFixedDtElapsed) {
    now = 0.125;

    EXPECT_EQ(engine.advance(), 0U);
    EXPECT_EQ(countOf(log, "preUpdate"), 1U);
    EXPECT_EQ(countOf(log, "fixedUpdate"), 0U);
    EXPECT_EQ(countOf(log, "update"), 1U) << "update runs every frame, even without a fixed step";
}

TEST_F(EngineAdvanceTest, RunsEveryDueStep) {
    now = 0.75;

    EXPECT_EQ(engine.advance(), 3U);
    EXPECT_EQ(countOf(log, "preUpdate"), 1U);
    EXPECT_EQ(countOf(log, "fixedUpdate"), 3U);
    EXPECT_EQ(countOf(log, "update"), 1U);
    EXPECT_EQ(engine.clock().time().tickCount, 3U);
}

TEST_F(EngineAdvanceTest, RunsPhasesInOrder) {
    now = 0.25;
    engine.advance();

    const std::vector<std::string> frame(log.begin() + 2, log.end());
    EXPECT_EQ(frame, (std::vector<std::string>{"preUpdate", "fixedUpdate", "update"}));
}

TEST_F(EngineAdvanceTest, KeepsTheRemainderForTheNextCall) {
    now = 0.375;
    EXPECT_EQ(engine.advance(), 1U);

    now = 0.5;
    EXPECT_EQ(engine.advance(), 1U) << "0.125 s left over + 0.125 s elapsed make one more step";
    EXPECT_EQ(engine.clock().time().tickCount, 2U);
}

TEST_F(EngineAdvanceTest, ReturnsZeroWhenNoTimeElapsed) {
    now = 0.25;
    EXPECT_EQ(engine.advance(), 1U);

    EXPECT_EQ(engine.advance(), 0U);
    EXPECT_EQ(countOf(log, "fixedUpdate"), 1U);
}

TEST_F(EngineAdvanceTest, CapsTheBacklog) {
    now = 100.0;

    EXPECT_EQ(engine.advance(), ecs::time::Clock::DEFAULT_MAX_STEPS_PER_TICK);
    EXPECT_EQ(countOf(log, "fixedUpdate"), ecs::time::Clock::DEFAULT_MAX_STEPS_PER_TICK);

    now = 100.25;
    EXPECT_EQ(engine.advance(), 1U) << "the dropped backlog is not replayed later";
}

TEST_F(EngineAdvanceTest, DoesNotRender) {
    now = 0.5;
    engine.advance();
    engine.advance();

    EXPECT_EQ(countOf(log, "render"), 0U);
}

TEST_F(EngineAdvanceTest, ClockResetDropsTheTimeSpentBeforeIt) {
    now = 60.0;  // e.g. a room waiting in the lobby
    engine.clock().reset();

    now = 60.25;
    EXPECT_EQ(engine.advance(), 1U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
