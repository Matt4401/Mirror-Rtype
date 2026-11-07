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
constexpr float BALL_RADIUS = 91.25F;

/** @brief Distance from a point to a segment. */
float segmentDistance(const float x, const float y, const float ax, const float ay, const float bx, const float by) {
    const float ex = bx - ax;
    const float ey = by - ay;
    const float along = std::clamp((((x - ax) * ex) + ((y - ay) * ey)) / ((ex * ex) + (ey * ey)), 0.0F, 1.0F);
    return std::hypot(x - (ax + (ex * along)), y - (ay + (ey * along)));
}

/**
 * @brief Distance from a point to the walls of the arena and the goals, positive inside. Written independently from
 * Arena.wren: the union of the rounded rectangle and the two goal boxes.
 */
float arenaDistance(const float x, const float y) {
    const float r = arena::CORNER_RADIUS;
    const float dx = x - std::clamp(x, arena::LEFT + r, arena::RIGHT - r);
    const float dy = y - std::clamp(y, arena::TOP + r, arena::BOTTOM - r);
    const bool inRoundedRect = (dx * dx) + (dy * dy) < r * r;
    const bool inGoal = y >= arena::GOAL_TOP && y <= arena::GOAL_BOTTOM &&
                        ((x >= arena::LEFT - arena::GOAL_DEPTH && x <= arena::LEFT) ||
                         (x >= arena::RIGHT && x <= arena::RIGHT + arena::GOAL_DEPTH));

    float nearest = std::min(segmentDistance(x, y, arena::LEFT + r, arena::TOP, arena::RIGHT - r, arena::TOP),
                             segmentDistance(x, y, arena::LEFT + r, arena::BOTTOM, arena::RIGHT - r, arena::BOTTOM));
    for (const float wall : {arena::LEFT, arena::RIGHT}) {
        const float back = wall == arena::LEFT ? wall - arena::GOAL_DEPTH : wall + arena::GOAL_DEPTH;
        nearest = std::min({nearest, segmentDistance(x, y, wall, arena::TOP + r, wall, arena::GOAL_TOP),
                            segmentDistance(x, y, back, arena::GOAL_TOP, wall, arena::GOAL_TOP),
                            segmentDistance(x, y, back, arena::GOAL_TOP, back, arena::GOAL_BOTTOM),
                            segmentDistance(x, y, back, arena::GOAL_BOTTOM, wall, arena::GOAL_BOTTOM)});
    }
    for (const float cx : {arena::LEFT + r, arena::RIGHT - r}) {
        for (const float cy : {arena::TOP + r, arena::BOTTOM - r}) {
            const bool sideX = cx < (arena::LEFT + arena::RIGHT) / 2.0F ? x < cx : x > cx;
            const bool sideY = cy < (arena::TOP + arena::BOTTOM) / 2.0F ? y < cy : y > cy;
            if (sideX && sideY) {
                nearest = std::min(nearest, std::fabs(std::hypot(x - cx, y - cy) - r));
            }
        }
    }
    return inRoundedRect || inGoal ? nearest : -nearest;
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
                                                             .cornerRadius = arena::CORNER_RADIUS,
                                                             .goalTop = arena::GOAL_TOP,
                                                             .goalBottom = arena::GOAL_BOTTOM,
                                                             .goalDepth = arena::GOAL_DEPTH});
        _registry.addComponent(arenaEntity, game::MatchState{});
        _arena = arenaEntity;
        _registry.addComponent(arenaEntity, ecs::ScriptComponent{.moduleName = std::string(game::MATCH_SCRIPT_MODULE),
                                                                 .className = "Match",
                                                                 .hasUpdate = true});
    }

    /** @brief Spawns a car at rest, wheels down. */
    void spawnCar(const float x, const float y, const float facing = 1.0F) {
        _car = _registry.spawnEntity();
        _registry.addComponent(_car, ecs::Transform{.x = x, .y = y, .scaleX = facing});
        input::InputState controls;
        controls.playerIndex = 3;
        _registry.addComponent(_car, std::move(controls));
        _registry.addComponent(_car, game::CarStatus{});
        _registry.addComponent(
            _car, ecs::ScriptComponent{.moduleName = std::string(game::CAR_SCRIPT_MODULE), .className = "Car"});
    }

    /** @brief Spawns the ball at rest. */
    void spawnBall(const float x, const float y) {
        _ball = _registry.spawnEntity();
        _registry.addComponent(_ball, ecs::Transform{.x = x, .y = y});
        _registry.addComponent(
            _ball, ecs::ScriptComponent{.moduleName = std::string(game::BALL_SCRIPT_MODULE), .className = "Ball"});
    }

    /** @brief Simulates `seconds` with the given input held. */
    void run(const float seconds, const float steer = 0.0F, const bool jump = false, const bool boost = false) {
        if (_car != 0) {
            auto& input = _registry.getComponents<input::InputState>().get(_car);
            input.setAxis(game::STEER_ID, steer);
            input.setDown(game::JUMP_ID, jump);
            input.setDown(game::BOOST_ID, boost);
        }
        for (float t = 0.0F; t < seconds; t += FRAME) {
            _scripts.update(FRAME);
            if (_car != 0) {
                ASSERT_TRUE(insideArena()) << "car left the arena at x=" << transform().x << " y=" << transform().y;
            }
            if (_ball != 0) {
                ASSERT_GE(arenaDistance(ball().x, ball().y), BALL_RADIUS - 1.0F)
                    << "ball left the arena at x=" << ball().x << " y=" << ball().y;
            }
        }
    }

    [[nodiscard]] const ecs::Transform& transform() { return _registry.getComponents<ecs::Transform>().get(_car); }
    [[nodiscard]] const ecs::Transform& ball() { return _registry.getComponents<ecs::Transform>().get(_ball); }
    [[nodiscard]] const game::CarStatus& status() { return _registry.getComponents<game::CarStatus>().get(_car); }

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

    /** @brief Gives the ball a velocity, through the shared Wren registry of the simulation. */
    void throwBall(const float vx, const float vy) {
        _scripts.update(0.0F);  // make sure the ball script exists
        const std::string code = "import \"rlss/scripts/Simulation\" for Simulation\nSimulation.ball.addVelocity(" +
                                 std::to_string(vx) + ", " + std::to_string(vy) + ")\n";
        ASSERT_TRUE(_scripts.getVm().interpret("rlss_test", code).has_value());
    }

    [[nodiscard]] const game::MatchState& match() { return _registry.getComponents<game::MatchState>().get(_arena); }

    ecs::Registry _registry;
    ecs::EventBus _bus;
    systems::ScriptSystem _scripts;
    ecs::Entity _arena = 0;
    ecs::Entity _car = 0;
    ecs::Entity _ball = 0;
};

TEST_F(CarPhysics, FallsAndRestsOnTheFloor) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - 300.0F);
    run(2.0F);
    EXPECT_NEAR(transform().y, arena::BOTTOM - HALF_HEIGHT, 1.0F);
    EXPECT_NEAR(transform().rotation, 0.0F, 0.01F);
}

TEST_F(CarPhysics, DrivesAndReachesTheThrottleTopSpeed) {
    spawnCar(arena::LEFT + 600.0F, arena::BOTTOM - HALF_HEIGHT);
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

TEST_F(CarPhysics, DrivesUpTheWallAboveTheGoalAndAlongTheCeilingForAMoment) {
    // On the right wall, above the goal, wheels against the wall and nose up.
    spawnCar(arena::RIGHT - HALF_HEIGHT + 1.0F, arena::GOAL_TOP - 80.0F);
    _registry.getComponents<ecs::Transform>().get(_car).rotation = -std::numbers::pi_v<float> / 2.0F;
    run(0.1F);
    float highest = arena::BOTTOM;
    for (int i = 0; i < 90; ++i) {
        run(FRAME, 1.0F, false, true);
        highest = std::min(highest, transform().y);
    }
    // Up the wall, through the curved corner, onto the ceiling: from a standstill on this short wall, it takes boost
    // to be fast enough for the corner to hold the car against gravity.
    EXPECT_LT(highest, arena::TOP + 60.0F);
}

TEST_F(CarPhysics, DrivingFastUpTheRampCarriesTheCarIntoTheGoalMouth) {
    spawnCar(arena::RIGHT - 900.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.1F);
    float highest = arena::BOTTOM;
    for (int i = 0; i < 120; ++i) {
        run(FRAME, 1.0F, false, true);
        highest = std::min(highest, transform().y);
    }
    // The ramp ends at the goal: there is no wall above it to drive on, the car flies up the mouth or into the goal.
    EXPECT_LT(highest, arena::GOAL_BOTTOM);
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

TEST_F(CarPhysics, BoostGoesPastTheThrottleTopSpeedAndEmptiesTheTank) {
    spawnCar(arena::LEFT + 600.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.2F);
    EXPECT_FLOAT_EQ(status().boost, 100.0F);
    float previous = transform().x;
    float fastest = 0.0F;
    for (int i = 0; i < 60; ++i) {
        run(FRAME, 0.0F, false, true);
        fastest = std::max(fastest, (transform().x - previous) / FRAME);
        previous = transform().x;
    }
    // One second of boost from rest: throttle (~1300 uu/s) + boost (~990 uu/s), well past the 1410 throttle cap.
    EXPECT_GT(fastest, 1600.0F);
    EXPECT_TRUE(status().boosting);
    EXPECT_NEAR(status().boost, 100.0F - (100.0F / 3.0F), 2.0F);
}

TEST_F(CarPhysics, BoostRefillsOnlyAgainstASurface) {
    spawnCar(arena::LEFT + 600.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.2F);
    run(0.6F, 0.0F, false, true);
    const float afterBoost = status().boost;
    run(1.0F);
    EXPECT_GT(status().boost, afterBoost + 15.0F);  // on the floor: refills

    run(0.2F, 0.0F, true);  // jump
    const float jumped = status().boost;
    run(0.3F);  // in the air: no refill
    EXPECT_NEAR(status().boost, jumped, 0.5F);
}

TEST_F(CarPhysics, BallBouncesLikeInRocketLeague) {
    spawnBall(arena::WIDTH / 2.0F, arena::BOTTOM - BALL_RADIUS - 500.0F);
    run(0.2F);
    // Falls 500 uu, bounces with restitution 0.6: back up to about 0.36 * 500 = 180 uu (a little less with drag).
    float bottom = arena::TOP;
    for (int i = 0; i < 120; ++i) {
        run(FRAME);
        bottom = std::max(bottom, ball().y);
    }
    EXPECT_NEAR(bottom, arena::BOTTOM - BALL_RADIUS, 1.0F);
    float apex = arena::BOTTOM;
    for (int i = 0; i < 60; ++i) {
        run(FRAME);
        apex = std::min(apex, ball().y);
    }
    const float bounce = arena::BOTTOM - BALL_RADIUS - apex;
    EXPECT_GT(bounce, 150.0F);
    EXPECT_LT(bounce, 185.0F);
    run(6.0F);
    EXPECT_NEAR(ball().y, arena::BOTTOM - BALL_RADIUS, 1.0F);  // settles on the floor
}

TEST_F(CarPhysics, DrivingIntoTheBallPushesIt) {
    spawnCar(arena::LEFT + 600.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::LEFT + 900.0F, arena::BOTTOM - BALL_RADIUS);
    run(0.3F);
    const float start = ball().x;
    run(1.0F, 1.0F);
    run(0.5F);
    EXPECT_GT(ball().x - start, 300.0F);
    EXPECT_GT(ball().x, transform().x + BALL_RADIUS);  // pushed ahead, not driven through
}

TEST_F(CarPhysics, AHitSendsTheBallFasterThanTheCar) {
    spawnCar(arena::LEFT + 600.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::RIGHT - 500.0F, arena::BOTTOM - BALL_RADIUS);
    run(0.2F);
    float carSpeed = 0.0F;
    float ballSpeed = 0.0F;
    float carX = transform().x;
    float ballX = ball().x;
    float ballY = ball().y;
    for (int i = 0; i < 120; ++i) {
        run(FRAME, 1.0F);
        const float speed = std::hypot(ball().x - ballX, ball().y - ballY) / FRAME;
        if (speed < 1.0F) {
            carSpeed = (transform().x - carX) / FRAME;  // until the ball moves
        }
        ballSpeed = std::max(ballSpeed, speed);
        carX = transform().x;
        ballX = ball().x;
        ballY = ball().y;
    }
    // Contact: the ball takes ~6/7 of the car speed; Rocket League's extra impulse adds ~0.6 of it along the line
    // between the centers. The bumper hits the ball under its center, so it also pops up.
    EXPECT_GT(carSpeed, 1000.0F);
    EXPECT_GT(ballSpeed, carSpeed * 1.2F);
    EXPECT_LT(ballSpeed, carSpeed * 1.7F);
}

TEST_F(CarPhysics, RandomPlayNeverLetsTheBallOrTheCarOut) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::WIDTH / 2.0F + 300.0F, arena::BOTTOM - 400.0F);
    std::mt19937 random(7);
    std::uniform_int_distribution<int> steer(-1, 1);
    std::bernoulli_distribution press(0.35);
    std::uniform_real_distribution<float> duration(0.05F, 0.8F);
    for (int i = 0; i < 400; ++i) {
        run(duration(random), static_cast<float>(steer(random)), press(random), press(random));
        if (HasFatalFailure()) {
            return;
        }
    }
}

TEST_F(CarPhysics, ABallBehindTheLeftGoalLineScoresForOrangeThenKicksOff) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);  // player 3: orange team
    spawnBall(arena::LEFT - 140.0F, arena::GOAL_BOTTOM - BALL_RADIUS);
    run(0.1F);
    EXPECT_EQ(match().orangeScore, 1);
    EXPECT_EQ(match().blueScore, 0);
    EXPECT_EQ(match().phase, 1);

    run(2.0F);  // celebration, then everybody back in place
    EXPECT_EQ(match().phase, 2);
    EXPECT_EQ(match().countdown, 3);
    EXPECT_NEAR(ball().x, (arena::LEFT + arena::RIGHT) / 2.0F, 0.01F);
    EXPECT_NEAR(ball().y, (arena::TOP + arena::BOTTOM) / 2.0F, 0.01F);
    // Player 3 is the second orange player: second kickoff spot on the right, facing left.
    EXPECT_NEAR(transform().x, arena::RIGHT - 600.0F - 160.0F, 0.01F);
    EXPECT_LT(transform().scaleX, 0.0F);

    const float frozenX = transform().x;
    run(2.5F, 1.0F, false, true);  // frozen during the countdown, whatever the input
    EXPECT_FLOAT_EQ(transform().x, frozenX);
    EXPECT_FLOAT_EQ(ball().y, (arena::TOP + arena::BOTTOM) / 2.0F);
    EXPECT_EQ(match().countdown, 1);

    run(0.8F, 1.0F);
    EXPECT_EQ(match().phase, 0);
    EXPECT_GT(transform().x, frozenX);                         // play resumed
    EXPECT_GT(ball().y, (arena::TOP + arena::BOTTOM) / 2.0F);  // the ball drops
    EXPECT_EQ(match().orangeScore, 1);                         // scored once
}

TEST_F(CarPhysics, AShotIntoTheRightGoalScoresForBlue) {
    spawnBall(arena::RIGHT - 600.0F, arena::GOAL_BOTTOM - 150.0F);
    throwBall(2000.0F, -200.0F);
    run(1.0F);
    EXPECT_EQ(match().blueScore, 1);
    EXPECT_EQ(match().orangeScore, 0);
}

TEST_F(CarPhysics, AShotAboveTheCrossbarBouncesBack) {
    spawnBall(arena::RIGHT - 600.0F, arena::GOAL_TOP - 200.0F);
    throwBall(2000.0F, 0.0F);
    run(1.0F);
    EXPECT_EQ(match().blueScore, 0);
    EXPECT_LT(ball().x, arena::RIGHT);
}

TEST_F(CarPhysics, AShotUnderTheGoalHitsTheRampAndStaysOut) {
    spawnBall(arena::RIGHT - 600.0F, arena::BOTTOM - BALL_RADIUS);
    throwBall(1500.0F, 0.0F);
    run(0.6F);
    EXPECT_EQ(match().blueScore, 0);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
