// Runs the Wren car physics (rlss/scripts/Car.wren) headless: no window, no network, scripted inputs.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <random>
#include <string>
#include <utility>

#include "Arena.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Tag.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "game/Components.hpp"
#include "game/ScriptBindings.hpp"
#include "input/InputState.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace {

namespace arena = rlss::arena;
namespace game = rlss::game;

constexpr float FRAME = 1.0F / 60.0F;
constexpr float HALF_LENGTH = 59.0F;
constexpr float HALF_HEIGHT = 18.0F;

/** @brief Distance from a point to the arena walls, positive inside (same shape as Arena.wren). */
float arenaDistance(const float x, const float y) {
    const float r = arena::CORNER_RADIUS;
    const float dx = x - std::clamp(x, arena::LEFT + r, arena::RIGHT - r);
    const float dy = y - std::clamp(y, arena::TOP + r, arena::BOTTOM - r);
    if (dx != 0.0F || dy != 0.0F) {
        return r - std::hypot(dx, dy);
    }
    return std::min({x - arena::LEFT, arena::RIGHT - x, y - arena::TOP, arena::BOTTOM - y});
}

class CarPhysics : public ::testing::Test {
  protected:
    void SetUp() override {
        std::filesystem::current_path(RLSS_PROJECT_DIR);
        _scripts.init(_registry, _bus);
        _scripts.enableHotReload(false);
        ASSERT_TRUE(game::installScriptBindings(_scripts));

        const ecs::Entity arenaEntity = _registry.spawnEntity();
        _registry.addComponent(arenaEntity, ecs::Tag{.name = std::string(game::ARENA_TAG)});
        _registry.addComponent(arenaEntity, game::ArenaShape{.left = arena::LEFT,
                                                             .top = arena::TOP,
                                                             .right = arena::RIGHT,
                                                             .bottom = arena::BOTTOM,
                                                             .cornerRadius = arena::CORNER_RADIUS});
    }

    /** @brief Spawns a car at rest, wheels down. */
    void spawnCar(const float x, const float y, const float facing = 1.0F) {
        _car = _registry.spawnEntity();
        _registry.addComponent(_car, ecs::Transform{.x = x, .y = y, .scaleX = facing});
        input::InputState controls;
        controls.playerIndex = 3;
        _registry.addComponent(_car, std::move(controls));
        _registry.addComponent(
            _car, ecs::ScriptComponent{
                      .moduleName = std::string(game::CAR_SCRIPT_MODULE), .className = "Car", .hasUpdate = true});
    }

    /** @brief Simulates `seconds` with the given input held. */
    void run(const float seconds, const float steer = 0.0F, const bool jump = false) {
        auto& input = _registry.getComponents<input::InputState>().get(_car);
        input.setAxis(game::STEER_ID, steer);
        input.setDown(game::JUMP_ID, jump);
        for (float t = 0.0F; t < seconds; t += FRAME) {
            _scripts.update(FRAME);
            ASSERT_TRUE(insideArena()) << "car left the arena at x=" << transform().x << " y=" << transform().y;
        }
    }

    [[nodiscard]] const ecs::Transform& transform() { return _registry.getComponents<ecs::Transform>().get(_car); }

    /** @brief Every corner of the hitbox is inside the arena (with a small tolerance). */
    [[nodiscard]] bool insideArena() {
        const ecs::Transform& t = transform();
        const float c = std::cos(t.rotation);
        const float s = std::sin(t.rotation);
        for (const float lx : {-HALF_LENGTH, HALF_LENGTH}) {
            for (const float ly : {-HALF_HEIGHT, HALF_HEIGHT}) {
                if (arenaDistance(t.x + (c * lx) + (s * ly), t.y + (s * lx) - (c * ly)) < -10.0F) {
                    return false;
                }
            }
        }
        return true;
    }

    ecs::Registry _registry;
    ecs::EventBus _bus;
    systems::ScriptSystem _scripts;
    ecs::Entity _car = 0;
};

TEST_F(CarPhysics, FallsAndRestsOnTheFloor) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - 300.0F);
    run(2.0F);
    EXPECT_NEAR(transform().y, arena::BOTTOM - HALF_HEIGHT, 1.0F);
    EXPECT_NEAR(transform().rotation, 0.0F, 0.01F);
}

TEST_F(CarPhysics, DrivesAndReachesTheThrottleTopSpeed) {
    spawnCar(arena::LEFT + 400.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.2F);
    const float start = transform().x;
    run(0.5F, 1.0F);
    const float travelled = transform().x - start;
    // Rocket League: ~1600 uu/s² at rest, so 0.5 s covers about 0.5 * 1600 * 0.25 = 200 uu (a bit less).
    EXPECT_GT(travelled, 150.0F);
    EXPECT_LT(travelled, 230.0F);
}

TEST_F(CarPhysics, SteeringBackTurnsTheCarAround) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.2F);
    run(0.3F, -1.0F);
    EXPECT_LT(transform().scaleX, 0.0F);
}

TEST_F(CarPhysics, DrivesUpTheWallAndAlongTheCeilingForAMoment) {
    spawnCar(arena::LEFT + 400.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.2F);
    float highest = arena::BOTTOM;
    bool climbedWall = false;
    for (int i = 0; i < 300; ++i) {
        run(FRAME, 1.0F);
        highest = std::min(highest, transform().y);
        climbedWall = climbedWall || (transform().x > arena::RIGHT - 60.0F && transform().y < arena::BOTTOM - 500.0F);
    }
    EXPECT_TRUE(climbedWall);
    // Fast enough through the corner to reach the ceiling.
    EXPECT_LT(highest, arena::TOP + 60.0F);
}

TEST_F(CarPhysics, JumpsAndLandsAgain) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.2F);
    run(0.25F, 0.0F, true);
    float highest = transform().y;
    for (int i = 0; i < 60; ++i) {
        run(FRAME);
        highest = std::min(highest, transform().y);
    }
    // Full single jump in Rocket League: about 230 uu high.
    const float height = arena::BOTTOM - HALF_HEIGHT - highest;
    EXPECT_GT(height, 180.0F);
    EXPECT_LT(height, 280.0F);
    run(1.5F);
    EXPECT_NEAR(transform().y, arena::BOTTOM - HALF_HEIGHT, 1.0F);
}

TEST_F(CarPhysics, DodgeGivesHorizontalSpeedAndSpins) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.2F);
    run(0.1F, 0.0F, true);
    run(0.1F);
    const float x = transform().x;
    run(0.05F, 1.0F, true);
    run(0.3F, 1.0F);
    EXPECT_GT(transform().x - x, 120.0F);
    EXPECT_GT(std::fabs(transform().rotation), 0.5F);
}

TEST_F(CarPhysics, RandomInputsNeverLeaveTheArena) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    std::mt19937 random(42);
    std::uniform_int_distribution<int> steer(-1, 1);
    std::bernoulli_distribution jump(0.3);
    std::uniform_real_distribution<float> duration(0.05F, 0.8F);
    for (int i = 0; i < 400; ++i) {
        run(duration(random), static_cast<float>(steer(random)), jump(random));
        if (HasFatalFailure()) {
            return;
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
