#include <gtest/gtest.h>

#include "components/Predicted.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "metrics/MetricsBoard.hpp"
#include "metrics/MetricsPlugin.hpp"
#include "metrics/ShipMetricsPlugin.hpp"

namespace {

// A power of two: exact in binary, so that a refresh period is exactly 32 frames.
constexpr double FRAME = 1.0 / 64.0;

/** @brief Client engine with the metrics and the ship line, on a fake clock. */
struct ShipSide {
    ShipSide() {
        engine.addPlugin<metrics::MetricsPlugin>();
        engine.addPlugin<client::ShipMetricsPlugin>();
        engine.init();
    }

    /** @brief Lets one refresh period elapse. */
    void refresh() {
        for (int frame = 0; frame < 32; ++frame) {
            now += FRAME;
            engine.advance();
        }
    }

    [[nodiscard]] const metrics::Metric* position() {
        return engine.registry().getResource<metrics::MetricsBoard>().find("Position");
    }

    double now = 0.0;
    engine::Engine engine{[this]() -> double { return now; }};
};

}  // namespace

TEST(ShipMetrics, RequiresTheMetricsPlugin) {
    engine::Engine engine;
    engine.addPlugin<client::ShipMetricsPlugin>();

    EXPECT_ANY_THROW(engine.init());
}

TEST(ShipMetrics, NoLineBeforeTheShipIsThere) {
    ShipSide side;
    side.refresh();

    EXPECT_EQ(side.position(), nullptr);
}

TEST(ShipMetrics, ShowsThePositionOfThePredictedShip) {
    ShipSide side;
    ecs::Registry& registry = side.engine.registry();
    const ecs::Entity ship = registry.spawnEntity();
    registry.addComponent(ship, ecs::Predicted{});
    registry.addComponent(ship, ecs::Transform{.x = 12.4F, .y = 33.6F});
    const ecs::Entity other = registry.spawnEntity();
    registry.addComponent(other, ecs::Transform{.x = 500.0F, .y = 500.0F});

    side.refresh();

    ASSERT_NE(side.position(), nullptr);
    EXPECT_EQ(side.position()->value, "12, 34");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
