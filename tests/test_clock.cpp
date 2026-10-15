#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"

namespace {

constexpr float FIXED_DT = ecs::time::DEFAULT_FIXED_DT;

struct FakeTime {
    double now = 0.0;
    ecs::time::Clock::TimeSource source() {
        return [this] { return now; };
    }
};

}  // namespace

TEST(Clock, NoStepBeforeOneFixedDt) {
    FakeTime fake;
    ecs::time::Clock clock(fake.source());

    fake.now = 0.010;
    std::size_t calls = 0;
    EXPECT_EQ(clock.tick(FIXED_DT, [&](const ecs::time::Time&) { ++calls; }), 0U);
    EXPECT_EQ(calls, 0U);
}

TEST(Clock, DeltaTimeIsConstant) {
    FakeTime fake;
    ecs::time::Clock clock(fake.source());

    fake.now = 0.05;
    bool allConstant = true;
    clock.tick(FIXED_DT, [&](const ecs::time::Time& tick) { allConstant = allConstant && tick.deltaTime == FIXED_DT; });
    EXPECT_TRUE(allConstant);
}

TEST(Clock, ResidualTimeIsCarriedOver) {
    FakeTime fake;
    ecs::time::Clock clock(fake.source());

    // Two ticks of 10 ms each: neither alone reaches 16.6 ms, together they do.
    fake.now = 0.010;
    EXPECT_EQ(clock.tick(FIXED_DT, [](const ecs::time::Time&) {}), 0U);
    fake.now = 0.020;
    EXPECT_EQ(clock.tick(FIXED_DT, [](const ecs::time::Time&) {}), 1U);
}

TEST(Clock, NoDriftOverTenThousandTicks) {
    FakeTime fake;
    ecs::time::Clock clock(fake.source());

    constexpr std::size_t TICKS = 10000;
    for (std::size_t i = 1; i <= TICKS; ++i) {
        fake.now = static_cast<double>(i) * static_cast<double>(FIXED_DT);
        EXPECT_EQ(clock.tick(FIXED_DT, [](const ecs::time::Time&) {}), 1U);
    }

    EXPECT_EQ(clock.time().tickCount, TICKS);
    EXPECT_NEAR(clock.time().totalElapsedTime, static_cast<float>(TICKS) * FIXED_DT, 0.01F);
}

TEST(Clock, SimulationIsIndependentOfFramerate) {
    constexpr double DURATION = 10.0;

    auto simulate = [&](double frameDuration) {
        FakeTime fake;
        ecs::time::Clock clock(fake.source(), 1000);
        std::uint64_t ticks = 0;
        for (double t = frameDuration; t <= DURATION + 1e-9; t += frameDuration) {
            fake.now = t;
            clock.tick(FIXED_DT, [&](const ecs::time::Time&) { ++ticks; });
        }
        return ticks;
    };

    const std::uint64_t fast = simulate(1.0 / 144.0);
    const std::uint64_t slow = simulate(1.0 / 20.0);
    EXPECT_LE(fast > slow ? fast - slow : slow - fast, 1U);
}

TEST(Clock, SpiralOfDeathIsCapped) {
    FakeTime fake;
    ecs::time::Clock clock(fake.source(), 5);

    fake.now = 10.0;  // simulated multi-second freeze
    std::size_t calls = 0;
    EXPECT_EQ(clock.tick(FIXED_DT, [&](const ecs::time::Time&) { ++calls; }), 5U);
    EXPECT_EQ(calls, 5U);

    // The backlog was dropped: the next short frame does not trigger a burst.
    fake.now = 10.001;
    EXPECT_EQ(clock.tick(FIXED_DT, [](const ecs::time::Time&) {}), 0U);
}

TEST(Clock, ResetClearsCounters) {
    FakeTime fake;
    ecs::time::Clock clock(fake.source());

    fake.now = 0.1;
    clock.tick(FIXED_DT, [](const ecs::time::Time&) {});
    ASSERT_GT(clock.time().tickCount, 0U);

    clock.reset();
    EXPECT_EQ(clock.time().tickCount, 0U);
    EXPECT_EQ(clock.time().totalElapsedTime, 0.0F);
}

TEST(Clock, InvalidFixedDtRunsNothing) {
    FakeTime fake;
    ecs::time::Clock clock(fake.source());

    fake.now = 1.0;
    EXPECT_EQ(clock.tick(0.0F, [](const ecs::time::Time&) {}), 0U);
    EXPECT_EQ(clock.tick(-1.0F, [](const ecs::time::Time&) {}), 0U);
}

TEST(Clock, DefaultSourceUsesRealTime) {
    ecs::time::Clock clock;
    std::size_t calls = 0;
    clock.tick(FIXED_DT, [&](const ecs::time::Time&) { ++calls; });
    EXPECT_LE(calls, ecs::time::Clock::DEFAULT_MAX_STEPS_PER_TICK);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
