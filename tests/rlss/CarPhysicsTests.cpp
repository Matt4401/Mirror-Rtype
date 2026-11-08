// Runs the Wren car physics (rlss/scripts/Car.wren) headless: no window, no network, scripted inputs.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
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
constexpr float BALL_RADIUS = rlss::BALL_RADIUS;
/** @brief Gap between the car's bumper and the ball in the drive / flip shot tests, whatever the ball size. */
constexpr float SHOT_GAP = 200.0F;
/** @brief Upper bound of the ball speed when a car drives into it from `SHOT_GAP` away (no flip). */
constexpr float DRIVE_SHOT_FROM_GAP = 800.0F;

/** @brief Distance from a point to a segment. */
float segmentDistance(const float x, const float y, const float ax, const float ay, const float bx, const float by) {
    const float ex = bx - ax;
    const float ey = by - ay;
    const float along = std::clamp((((x - ax) * ex) + ((y - ay) * ey)) / ((ex * ex) + (ey * ey)), 0.0F, 1.0F);
    return std::hypot(x - (ax + (ex * along)), y - (ay + (ey * along)));
}

/** @brief Height of the ramp top (the top of the lower curved corners) above which the goal mouth starts. */
constexpr float RAMP_TOP = arena::BOTTOM - arena::CORNER_RADIUS;

/** @brief How far inside the arena the bottom posts stand: where the goal bottom cuts the curved corner. */
float postInset() {
    const float drop = arena::GOAL_BOTTOM - RAMP_TOP;
    const float r = arena::CORNER_RADIUS;
    return drop > 0.0F ? r - std::sqrt((r * r) - (drop * drop)) : 0.0F;
}

/**
 * @brief True if a point is inside a goal box whose two back corners are rounded (`arena::GOAL_CORNER_RADIUS`), and
 * whose mouth reaches the bottom post below the top of the ramp.
 * @param wall Side wall the goal opens in.
 * @param outward -1 for the left goal, +1 for the right one.
 */
bool inRoundedGoal(const float x, const float y, const float wall, const float outward) {
    const float back = wall + (outward * arena::GOAL_DEPTH);
    const float mouth = wall - (outward * (y >= RAMP_TOP ? postInset() : 0.0F));
    if (y < arena::GOAL_TOP || y > arena::GOAL_BOTTOM || x < std::min(mouth, back) || x > std::max(mouth, back)) {
        return false;
    }
    const float rr = arena::GOAL_CORNER_RADIUS;
    const float cx = back - (outward * rr);
    const float cy = std::clamp(y, arena::GOAL_TOP + rr, arena::GOAL_BOTTOM - rr);
    if ((x - cx) * outward <= 0.0F || cy == y) {
        return true;  // not in a corner square
    }
    return std::hypot(x - cx, y - cy) <= rr;
}

/**
 * @brief Distance from a point to the walls of the arena and the goals, positive inside. Written independently from
 * Arena.wren: the union of the rounded rectangle and the two goal boxes with rounded back corners.
 */
float arenaDistance(const float x, const float y) {
    const float r = arena::CORNER_RADIUS;
    const float dx = x - std::clamp(x, arena::LEFT + r, arena::RIGHT - r);
    const float dy = y - std::clamp(y, arena::TOP + r, arena::BOTTOM - r);
    const bool inRoundedRect = (dx * dx) + (dy * dy) < r * r;
    const bool inGoal = inRoundedGoal(x, y, arena::LEFT, -1.0F) || inRoundedGoal(x, y, arena::RIGHT, 1.0F);

    float nearest = std::min(segmentDistance(x, y, arena::LEFT + r, arena::TOP, arena::RIGHT - r, arena::TOP),
                             segmentDistance(x, y, arena::LEFT + r, arena::BOTTOM, arena::RIGHT - r, arena::BOTTOM));
    const float rr = arena::GOAL_CORNER_RADIUS;
    for (const float wall : {arena::LEFT, arena::RIGHT}) {
        const float outward = wall == arena::LEFT ? -1.0F : 1.0F;
        const float back = wall + (outward * arena::GOAL_DEPTH);
        const float inner = back - (outward * rr);  // where the crossbar and the bottom turn into the corners
        const float post = wall - (outward * postInset());
        nearest = std::min({nearest, segmentDistance(x, y, wall, arena::TOP + r, wall, arena::GOAL_TOP),
                            segmentDistance(x, y, inner, arena::GOAL_TOP, wall, arena::GOAL_TOP),
                            segmentDistance(x, y, back, arena::GOAL_TOP + rr, back, arena::GOAL_BOTTOM - rr),
                            segmentDistance(x, y, inner, arena::GOAL_BOTTOM, post, arena::GOAL_BOTTOM)});
        if (arena::GOAL_BOTTOM < RAMP_TOP) {
            nearest = std::min(nearest, segmentDistance(x, y, wall, arena::GOAL_BOTTOM, wall, RAMP_TOP));
        }
        for (const float cy : {arena::GOAL_TOP + rr, arena::GOAL_BOTTOM - rr}) {
            const float sideY = cy < (arena::GOAL_TOP + arena::GOAL_BOTTOM) / 2.0F ? -1.0F : 1.0F;
            if ((x - inner) * outward > 0.0F && (y - cy) * sideY > 0.0F) {
                nearest = std::min(nearest, std::fabs(std::hypot(x - inner, y - cy) - rr));
            }
        }
    }
    for (const float cx : {arena::LEFT + r, arena::RIGHT - r}) {
        for (const float cy : {arena::TOP + r, arena::BOTTOM - r}) {
            const bool sideX = cx < (arena::LEFT + arena::RIGHT) / 2.0F ? x < cx : x > cx;
            const bool sideY = cy < (arena::TOP + arena::BOTTOM) / 2.0F ? y < cy : y > cy;
            const float length = std::hypot(x - cx, y - cy);
            // The goals cut the top of the lower curves: they only go up to the goal bottom.
            const bool cut = cy > arena::TOP + r && cy + ((y - cy) * r / length) < arena::GOAL_BOTTOM;
            if (sideX && sideY && !cut) {
                nearest = std::min(nearest, std::fabs(length - r));
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
                                                             .goalDepth = arena::GOAL_DEPTH,
                                                             .goalCornerRadius = arena::GOAL_CORNER_RADIUS});
        // Most tests want the ball in play at once: a match already running (a new one starts with a kickoff).
        _registry.addComponent(arenaEntity, game::MatchState{.clock = MATCH_DURATION});
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
        _registry.addComponent(_car, game::CarCues{});
        _registry.addComponent(
            _car, ecs::ScriptComponent{.moduleName = std::string(game::CAR_SCRIPT_MODULE), .className = "Car"});
    }

    /** @brief Spawns the ball at rest. */
    void spawnBall(const float x, const float y) {
        _ball = _registry.spawnEntity();
        _registry.addComponent(_ball, ecs::Transform{.x = x, .y = y});
        _registry.addComponent(_ball, game::ShotGlow{});
        _registry.addComponent(_ball, game::BallCues{});
        _registry.addComponent(
            _ball, ecs::ScriptComponent{.moduleName = std::string(game::BALL_SCRIPT_MODULE), .className = "Ball"});
    }

    /** @brief Simulates `seconds` with the given input held. */
    void run(const float seconds, const float steer = 0.0F, const bool jump = false, const bool boost = false,
             const float pitch = 0.0F) {
        if (_car != 0) {
            auto& input = _registry.getComponents<input::InputState>().get(_car);
            input.setAxis(game::STEER_ID, steer);
            input.setAxis(game::PITCH_ID, pitch);
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

    /**
     * @brief False if the car touches a surface with its roof facing it (lying on its back). The surface normal is the
     * gradient of the test's own arena distance.
     */
    [[nodiscard]] bool notOnItsBack() {
        const ecs::Transform& t = transform();
        const float c = std::cos(t.rotation);
        const float s = std::sin(t.rotation);
        float closest = std::numeric_limits<float>::max();
        for (const float lx : {-HALF_LENGTH, HALF_LENGTH}) {
            for (const float ly : {-HALF_HEIGHT, HALF_HEIGHT}) {
                closest = std::min(closest, arenaDistance(t.x + (c * lx) + (s * ly), t.y + (s * lx) - (c * ly)));
            }
        }
        if (closest > 2.0F) {
            return true;  // not touching anything
        }
        constexpr float STEP = 1.0F;
        const float nx = arenaDistance(t.x + STEP, t.y) - arenaDistance(t.x - STEP, t.y);
        const float ny = arenaDistance(t.x, t.y + STEP) - arenaDistance(t.x, t.y - STEP);
        const float length = std::hypot(nx, ny);
        if (length < 0.0001F) {
            return true;
        }
        const float roofTowardWall = -((s * nx) - (c * ny)) / length;  // roof direction against the normal
        return roofTowardWall < 0.5F;
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

  public:
    /** @brief Fastest speed of the ball over `seconds` with the given input held, from its frame to frame motion. */
    float measureFastestBall(const float seconds, const float steer, const bool jump, const bool boost) {
        float fastest = 0.0F;
        float x = ball().x;
        float y = ball().y;
        for (float t = 0.0F; t < seconds; t += FRAME) {
            run(FRAME, steer, jump, boost);
            fastest = std::max(fastest, std::hypot(ball().x - x, ball().y - y) / FRAME);
            x = ball().x;
            y = ball().y;
        }
        return fastest;
    }

  protected:
    /** @brief Runs Wren code in the simulation's VM (scripts must exist: call after a first update). */
    void wren(const std::string& code) {
        // A module of its own each time: two snippets (or a snippet and `throwBall`) may declare the same variables.
        const std::string module = "rlss_snippet_" + std::to_string(++_snippets);
        ASSERT_TRUE(_scripts.getVm()
                        .interpret(module, "import \"rlss/scripts/Simulation\" for Simulation\n" + code + "\n")
                        .has_value());
    }

    /** @brief Snippets run by `wren`, to name their modules. */
    int _snippets = 0;

    [[nodiscard]] int glow() { return _registry.getComponents<game::ShotGlow>().get(_ball).shot; }
    [[nodiscard]] const game::CarCues& carCues() { return _registry.getComponents<game::CarCues>().get(_car); }
    [[nodiscard]] const game::BallCues& ballCues() { return _registry.getComponents<game::BallCues>().get(_ball); }

    /**
     * @brief Car standing on its rear, right next to the ball on the floor, its wheels against the ball's right side.
     */
    void spawnCarWheelsAgainstBall() {
        const float ballY = arena::BOTTOM - BALL_RADIUS;
        spawnBall(arena::WIDTH / 2.0F, ballY);
        spawnCar((arena::WIDTH / 2.0F) + BALL_RADIUS + HALF_HEIGHT - 1.0F, ballY);
        _registry.getComponents<ecs::Transform>().get(_car).rotation = std::numbers::pi_v<float> / 2.0F;  // wheels left
    }

    [[nodiscard]] const game::MatchState& match() { return _registry.getComponents<game::MatchState>().get(_arena); }

    /** @brief The arena's `MatchState`, to set before the first `run()`: the match script starts from it. */
    [[nodiscard]] game::MatchState& matchState() { return _registry.getComponents<game::MatchState>().get(_arena); }

    /** @brief `Match.DURATION`: 3 minutes. */
    static constexpr int MATCH_DURATION = 180;

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
    // Sideswipe jumps are short: about 160 uu with the stronger gravity (230 uu in Rocket League).
    const float height = arena::BOTTOM - HALF_HEIGHT - highest;
    EXPECT_GT(height, 130.0F);
    EXPECT_LT(height, 200.0F);
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
        ASSERT_TRUE(notOnItsBack()) << "lying on its back at x=" << transform().x << " y=" << transform().y;
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

TEST_F(CarPhysics, BallBouncesAndSettles) {
    const float floor = arena::BOTTOM - BALL_RADIUS;
    spawnBall(arena::WIDTH / 2.0F, floor - 500.0F);
    // Falls 500 uu, bounces with restitution 0.6: back up to about 0.36 * 500 = 180 uu (a little less with drag).
    // A bounce is seen as the ball going down then up (it may leave the floor within the frame it touched it).
    bool landed = false;
    bool rose = false;
    float apex = floor;
    float previous = ball().y;
    float previousStep = 0.0F;
    for (int i = 0; i < 600; ++i) {
        run(FRAME);
        const float step = ball().y - previous;
        previous = ball().y;
        const bool bounced = previousStep > 0.0F && step < 0.0F;
        previousStep = step;
        if (!landed) {
            landed = bounced;
            continue;
        }
        apex = std::min(apex, ball().y);
        rose = true;
        if (bounced) {
            break;  // second bounce
        }
    }
    ASSERT_TRUE(landed && rose);
    const float bounce = floor - apex;
    EXPECT_GT(bounce, 150.0F);
    EXPECT_LT(bounce, 185.0F);
    run(6.0F);
    EXPECT_NEAR(ball().y, floor, 1.0F);  // settles on the floor
}

TEST_F(CarPhysics, DrivingIntoTheBallPushesIt) {
    spawnCar(arena::LEFT + 600.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::LEFT + 900.0F, arena::BOTTOM - BALL_RADIUS);
    run(0.3F);
    const float start = ball().x;
    run(1.0F, 1.0F);
    run(0.5F);
    EXPECT_GT(ball().x - start, 300.0F);
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
    // The contact gives the ball about the car speed along the contact normal, the extra impulse a bit more. The bumper
    // hits the ball under its center, so it also pops up.
    EXPECT_GT(carSpeed, 1000.0F);
    EXPECT_GT(ballSpeed, carSpeed * 0.8F);
    EXPECT_LT(ballSpeed, carSpeed * 1.4F);
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
        ASSERT_TRUE(notOnItsBack()) << "lying on its back at x=" << transform().x << " y=" << transform().y;
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

TEST_F(CarPhysics, AGoalExplodesTheBallAndBlowsTheCloseCarsAway) {
    // A car in the air in front of the left goal, falling straight down without the blast.
    const float startX = arena::LEFT + 160.0F;
    spawnCar(startX, arena::GOAL_BOTTOM - 150.0F);
    spawnBall(arena::LEFT - 140.0F, arena::GOAL_BOTTOM - 120.0F);
    run(0.1F);
    ASSERT_EQ(match().phase, 1);
    const float ballX = ball().x;
    const float ballY = ball().y;

    run(0.5F);
    EXPECT_GT(transform().x, startX + 100.0F);  // blown away from the goal
    run(1.0F);
    EXPECT_EQ(match().phase, 1);
    EXPECT_FLOAT_EQ(ball().x, ballX);  // out of play: it does not fall any more
    EXPECT_FLOAT_EQ(ball().y, ballY);
}

TEST_F(CarPhysics, ANewMatchStartsWithAKickoffAndAFullClock) {
    matchState() = game::MatchState{};  // no clock yet: a new match
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::WIDTH / 2.0F, arena::HEIGHT / 2.0F);
    run(0.1F);
    EXPECT_EQ(match().phase, 2);
    EXPECT_EQ(match().countdown, 3);
    EXPECT_EQ(match().clock, MATCH_DURATION);

    run(3.0F);
    EXPECT_EQ(match().phase, 0);
    EXPECT_EQ(match().clock, MATCH_DURATION);  // the countdown does not count
    run(10.0F);
    EXPECT_NEAR(match().clock, MATCH_DURATION - 10, 1);
    EXPECT_FALSE(match().overtime);
}

TEST_F(CarPhysics, TheClockStopsDuringAGoalAndItsKickoff) {
    matchState().clock = 60;
    spawnBall(arena::LEFT - 140.0F, arena::GOAL_BOTTOM - BALL_RADIUS);
    run(0.1F);
    ASSERT_EQ(match().phase, 1);
    run(4.5F);  // 2 s of celebration, then the countdown
    EXPECT_EQ(match().phase, 2);
    EXPECT_EQ(match().clock, 60);
    run(2.0F);
    EXPECT_EQ(match().phase, 0);
    EXPECT_EQ(match().clock, 59);
}

TEST_F(CarPhysics, TheMatchEndsWhenTimeRunsOutWithALead) {
    matchState().clock = 2;
    matchState().blueScore = 1;
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::WIDTH / 2.0F, arena::BOTTOM - BALL_RADIUS);
    run(1.2F);
    EXPECT_EQ(match().phase, 0);
    EXPECT_EQ(match().clock, 1);

    run(1.0F);
    EXPECT_EQ(match().phase, 3);
    EXPECT_EQ(match().clock, 0);
    EXPECT_FALSE(match().overtime);
    EXPECT_EQ(match().blueScore, 1);

    // Over: everything stays frozen, whatever the input.
    const float x = transform().x;
    run(1.0F, 1.0F, true, true);
    EXPECT_FLOAT_EQ(transform().x, x);
    EXPECT_EQ(match().phase, 3);
}

TEST_F(CarPhysics, ATieAtTheEndGoesToOvertimeAndTheNextGoalWins) {
    matchState().clock = 1;
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::WIDTH / 2.0F, arena::BOTTOM - BALL_RADIUS);
    run(1.2F);
    EXPECT_TRUE(match().overtime);
    EXPECT_EQ(match().phase, 2);  // overtime starts with a kickoff
    EXPECT_EQ(match().clock, 0);

    run(5.0F);  // 3 s of countdown, then the clock counts the overtime up
    EXPECT_EQ(match().phase, 0);
    EXPECT_NEAR(match().clock, 2, 1);

    wren("Simulation.ball.respawn(" + std::to_string(arena::LEFT - 140.0F) + ", " +
         std::to_string(arena::GOAL_BOTTOM - BALL_RADIUS) + ")");
    run(0.1F);
    EXPECT_EQ(match().orangeScore, 1);
    EXPECT_EQ(match().phase, 1);  // celebration first...
    run(2.5F);
    EXPECT_EQ(match().phase, 3);  // ...then the golden goal ends the match, no kickoff
    EXPECT_TRUE(match().overtime);
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

/** @brief Fastest speed of the ball over `seconds`, from its frame to frame motion. */
float fastestBall(CarPhysics& test, float seconds, float steer, bool jump = false, bool boost = false);

TEST_F(CarPhysics, BoostingNoseUpClimbsAgainstGravity) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - 600.0F);
    _registry.getComponents<ecs::Transform>().get(_car).rotation = -std::numbers::pi_v<float> / 2.0F;  // nose up
    run(FRAME, 0.0F, false, true);
    const float start = transform().y;
    run(0.6F, 0.0F, false, true);
    EXPECT_LT(transform().y, start);  // the boost beats gravity: it rises
}

TEST_F(CarPhysics, DrivesUpsideDownOnTheCeilingWhileDrivingAndFallsWhenLettingGo) {
    spawnCar(arena::WIDTH / 2.0F, arena::TOP + HALF_HEIGHT - 1.0F);
    _registry.getComponents<ecs::Transform>().get(_car).rotation = std::numbers::pi_v<float>;  // wheels up
    run(0.8F, 1.0F);
    EXPECT_LT(transform().y, arena::TOP + 30.0F);  // still on the ceiling after 0.8 s of driving
    run(1.0F);
    EXPECT_GT(transform().y, arena::TOP + 200.0F);  // let go: falls
}

TEST_F(CarPhysics, TheFlipStaysAvailableUntilTheCarTouchesASurface) {
    spawnCar(arena::WIDTH / 2.0F, arena::TOP + HALF_HEIGHT - 1.0F);
    _registry.getComponents<ecs::Transform>().get(_car).rotation = std::numbers::pi_v<float>;
    run(0.5F, 1.0F);
    run(FRAME, 1.0F, true);  // tap jump: off the ceiling, downward
    run(1.3F);               // long fall: Rocket League would have closed the flip window (1.25 s)
    ASSERT_GT(transform().y, arena::TOP + 300.0F);
    ASSERT_LT(transform().y, arena::BOTTOM - 100.0F);  // still in the air
    const float before = transform().rotation;
    run(0.1F, 1.0F, true);  // second press with a direction: dodge
    EXPECT_GT(std::fabs(std::remainder(transform().rotation - before, 2.0F * std::numbers::pi_v<float>)), 0.35F);
}

TEST_F(CarPhysics, TheFrontAtFullSpeedHitsMuchHarderThanTheRoof) {
    // The ball rests on the roof of a car that then drives off at full throttle.
    spawnCar(arena::LEFT + 800.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::LEFT + 800.0F, arena::BOTTOM - (2.0F * HALF_HEIGHT) - BALL_RADIUS - 2.0F);
    run(0.5F);
    const float roof = fastestBall(*this, 1.0F, 1.0F);
    // Same car, same throttle, hitting the ball with its front at full speed.
    spawnBall(arena::RIGHT - 600.0F, arena::BOTTOM - BALL_RADIUS);
    const float front = fastestBall(*this, 2.0F, 1.0F);
    EXPECT_GT(front, 1000.0F);
    EXPECT_LT(roof, front * 0.5F);
}

TEST_F(CarPhysics, AFlipIntoTheBallHitsMuchHarderThanDriving) {
    spawnCar(arena::LEFT + 800.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::LEFT + 800.0F + HALF_LENGTH + SHOT_GAP + BALL_RADIUS, arena::BOTTOM - BALL_RADIUS);
    run(0.3F);
    run(0.1F, 0.0F, true);  // jump
    run(0.05F);
    const float flip = fastestBall(*this, 1.5F, 1.0F, true);  // dodge forward into the ball
    EXPECT_GT(flip, 1.4F * DRIVE_SHOT_FROM_GAP);
}

TEST_F(CarPhysics, DrivingIntoTheBallFromCloseIsAWeakShot) {
    spawnCar(arena::LEFT + 800.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::LEFT + 800.0F + HALF_LENGTH + SHOT_GAP + BALL_RADIUS, arena::BOTTOM - BALL_RADIUS);
    run(0.3F);
    EXPECT_LT(fastestBall(*this, 1.0F, 1.0F), DRIVE_SHOT_FROM_GAP);
}

TEST_F(CarPhysics, ARollingBallStops) {
    spawnBall(arena::WIDTH / 2.0F, arena::BOTTOM - BALL_RADIUS);
    run(0.1F);
    throwBall(900.0F, 0.0F);
    run(6.0F);
    const float x = ball().x;
    run(0.5F);
    EXPECT_NEAR(ball().x, x, 1.0F);
}

TEST_F(CarPhysics, TheBallSpeedIsCapped) {
    spawnBall(arena::WIDTH / 2.0F, arena::BOTTOM - 700.0F);
    run(0.1F);
    throwBall(9000.0F, 0.0F);
    const float fastest = fastestBall(*this, 0.2F, 0.0F);
    EXPECT_LE(fastest, 2600.0F + 30.0F);
    EXPECT_GT(fastest, 2300.0F);
}

float fastestBall(CarPhysics& test, const float seconds, const float steer, const bool jump, const bool boost) {
    return test.measureFastestBall(seconds, steer, jump, boost);
}

TEST_F(CarPhysics, DroppedUpsideDownItTurnsOverAndLandsOnItsWheels) {
    const float floor = arena::BOTTOM - HALF_HEIGHT;
    spawnCar(arena::WIDTH / 2.0F, floor - 400.0F);
    _registry.getComponents<ecs::Transform>().get(_car).rotation = std::numbers::pi_v<float>;  // roof down
    for (int i = 0; i < 90; ++i) {
        run(FRAME);
        // Close enough to touch the floor with a corner: the wheels already point down.
        if (transform().y > floor - HALF_LENGTH) {
            ASSERT_LT(std::fabs(std::remainder(transform().rotation, 2.0F * std::numbers::pi_v<float>)),
                      std::numbers::pi_v<float> / 2.0F)
                << "frame " << i;
        }
    }
    EXPECT_NEAR(transform().y, floor, 1.0F);
    EXPECT_NEAR(std::remainder(transform().rotation, 2.0F * std::numbers::pi_v<float>), 0.0F, 0.01F);
    const float x = transform().x;
    run(0.5F, 1.0F);
    EXPECT_GT(transform().x, x + 50.0F);  // on its wheels: it drives
}

TEST_F(CarPhysics, AFlipIntoTheBallWithTheNoseIsARedShot) {
    spawnCar(arena::LEFT + 800.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::LEFT + 1150.0F, arena::BOTTOM - BALL_RADIUS);
    run(0.3F);
    run(0.1F, 0.0F, true);
    run(0.05F);
    (void)fastestBall(*this, 0.5F, 1.0F, true);
    EXPECT_EQ(glow(), static_cast<int>(game::Shot::Red));
}

TEST_F(CarPhysics, DrivingIntoTheBallIsNotAColoredShot) {
    spawnCar(arena::LEFT + 800.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::LEFT + 1150.0F, arena::BOTTOM - BALL_RADIUS);
    run(1.0F, 1.0F);
    EXPECT_EQ(glow(), static_cast<int>(game::Shot::None));
}

TEST_F(CarPhysics, JumpingOffTheBallWithTheWheelsIsAPurpleShot) {
    spawnCarWheelsAgainstBall();
    run(FRAME);              // the wheels touch the ball
    run(FRAME, 0.0F, true);  // jump without direction
    const float speed = fastestBall(*this, 0.2F, 0.0F);
    EXPECT_EQ(glow(), static_cast<int>(game::Shot::Purple));
    EXPECT_GT(speed, 700.0F);
    EXPECT_LT(ball().x, arena::WIDTH / 2.0F);                                     // pushed away from the wheels (left)
    EXPECT_GT(transform().x, (arena::WIDTH / 2.0F) + BALL_RADIUS + HALF_HEIGHT);  // the car jumped the other way
}

TEST_F(CarPhysics, JumpingOffTheBallWithADirectionFlipsAndMissesIt) {
    spawnCarWheelsAgainstBall();
    run(FRAME);
    run(FRAME, 1.0F, true);  // jump with a direction: a flip
    const float speed = fastestBall(*this, 0.2F, 1.0F);
    EXPECT_NE(glow(), static_cast<int>(game::Shot::Purple));
    EXPECT_LT(speed, 300.0F);
}

TEST_F(CarPhysics, TheWheelsOnTheBallResetTheFlipAndRefillBoost) {
    const float ballY = arena::BOTTOM - BALL_RADIUS;
    spawnBall(arena::WIDTH / 2.0F, ballY);
    spawnCar(arena::WIDTH / 2.0F, ballY - BALL_RADIUS - HALF_HEIGHT - 120.0F);  // above the ball, wheels down
    run(FRAME);
    wren("Simulation.cars[0].boost = 40");
    run(FRAME, 0.0F, true);  // double jump: the flip is used
    run(FRAME);
    EXPECT_FALSE(status().flipReset);
    bool reset = false;
    for (int i = 0; i < 90 && !reset; ++i) {
        run(FRAME);
        reset = status().flipReset;  // falls back onto the ball, wheels first
    }
    ASSERT_TRUE(reset);
    EXPECT_GE(status().boost, 40.0F + 25.0F);  // at least a quarter of the tank on a new touch
    // The flip is back: a flip with a direction now spins the car.
    const float before = transform().rotation;
    run(0.1F, 1.0F, true);
    EXPECT_GT(std::fabs(std::remainder(transform().rotation - before, 2.0F * std::numbers::pi_v<float>)), 0.35F);
    EXPECT_FALSE(status().flipReset);  // used
}

TEST_F(CarPhysics, FlippingTheRearIntoTheBallIsAGoldLob) {
    const float y = arena::BOTTOM - 500.0F;
    spawnCar(arena::WIDTH / 2.0F, y);  // facing right
    spawnBall((arena::WIDTH / 2.0F) - HALF_LENGTH - BALL_RADIUS - 40.0F, y);
    run(FRAME);
    throwBall(900.0F, 0.0F);  // the ball comes from behind
    run(FRAME, 1.0F, true);   // front flip, away from the ball: the rear swings up into it
    float lowestVy = 0.0F;
    float previousY = ball().y;
    for (int i = 0; i < 30; ++i) {
        run(FRAME, 1.0F);
        lowestVy = std::min(lowestVy, (ball().y - previousY) / FRAME);
        previousY = ball().y;
    }
    EXPECT_EQ(glow(), static_cast<int>(game::Shot::Gold));
    EXPECT_LT(lowestVy, -400.0F);  // lobbed upward
}

// --- Sound cues: what the clients hear comes from these counters ---

TEST_F(CarPhysics, JumpsAreCountedWithTheirKindAndTheLandingIsAnImpact) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.3F);
    const int landed = carCues().impacts;  // spawned a little above the floor
    EXPECT_EQ(carCues().jumps, 0);
    run(0.1F, 0.0F, true);
    EXPECT_EQ(carCues().jumps, 1);
    EXPECT_EQ(carCues().jumpKind, static_cast<int>(game::JumpKind::Jump));
    run(0.05F);
    run(0.05F, 1.0F, true);  // flip
    EXPECT_EQ(carCues().jumps, 2);
    EXPECT_EQ(carCues().jumpKind, static_cast<int>(game::JumpKind::Dodge));
    run(1.5F);
    EXPECT_GT(carCues().impacts, landed);
    EXPECT_GT(carCues().impactSpeed, 250.0F);
}

TEST_F(CarPhysics, ADoubleJumpIsCountedAsSuch) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.3F);
    run(0.1F, 0.0F, true);
    run(0.05F);
    run(0.05F, 0.0F, true);
    EXPECT_EQ(carCues().jumps, 2);
    EXPECT_EQ(carCues().jumpKind, static_cast<int>(game::JumpKind::DoubleJump));
}

TEST_F(CarPhysics, DrivingAroundTheArenaMakesNoImpact) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.3F);
    const int landed = carCues().impacts;
    run(1.0F, 1.0F);
    run(1.0F, -1.0F);
    EXPECT_EQ(carCues().impacts, landed);
    EXPECT_EQ(carCues().jumps, 0);
}

TEST_F(CarPhysics, BallBouncesAreCountedAndASettledBallIsQuiet) {
    spawnBall(arena::WIDTH / 2.0F, arena::BOTTOM - BALL_RADIUS - 500.0F);
    run(1.5F);
    EXPECT_GE(ballCues().bounces, 1);
    EXPECT_GT(ballCues().bounceSpeed, 300.0F);
    run(6.0F);
    const int settled = ballCues().bounces;
    run(2.0F);
    EXPECT_EQ(ballCues().bounces, settled);
    EXPECT_EQ(ballCues().hits, 0);
}

TEST_F(CarPhysics, AShotIsCountedWithItsStrength) {
    spawnCar(arena::LEFT + 800.0F, arena::BOTTOM - HALF_HEIGHT);
    spawnBall(arena::LEFT + 1150.0F, arena::BOTTOM - BALL_RADIUS);
    run(0.3F);
    run(0.1F, 0.0F, true);
    run(0.05F);
    (void)fastestBall(*this, 0.5F, 1.0F, true);
    EXPECT_GE(ballCues().hits, 1);
    EXPECT_GT(ballCues().hitSpeed, 1000.0F);
}

TEST_F(CarPhysics, SoundCountersGoOnFromTheComponent) {
    // A hot-reload recreates the script: the counters must go on, not restart and replay a sound on the clients.
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    _registry.getComponents<game::CarCues>().get(_car).jumps = 7;
    run(0.3F);
    EXPECT_EQ(carCues().jumps, 7);
    run(0.1F, 0.0F, true);
    EXPECT_EQ(carCues().jumps, 8);
}

// --- Rounded goal corners ---

TEST_F(CarPhysics, TheBackCornersOfTheGoalsAreRounded) {
    run(FRAME);  // the match script builds the arena
    // Just inside the sharp corner of a box: outside once the corner is rounded. The middle of the back stays a wall.
    const float back = arena::LEFT - arena::GOAL_DEPTH;
    wren(
        "import \"rlss/scripts/Arena\" for Arena\n"
        "var arena = Arena.fromWorld()\n"
        "if (arena.goalCornerRadius != " +
        std::to_string(arena::GOAL_CORNER_RADIUS) +
        ") Fiber.abort(\"radius\")\n"
        "arena.sample(" +
        std::to_string(back + 10.0F) + ", " + std::to_string(arena::GOAL_TOP + 10.0F) +
        ")\n"
        "if (arena.distance >= 0) Fiber.abort(\"sharp corner\")\n"
        "if (arena.curvature <= 0) Fiber.abort(\"not on the arc\")\n"
        "arena.sample(" +
        std::to_string(back + 10.0F) + ", " + std::to_string((arena::GOAL_TOP + arena::GOAL_BOTTOM) / 2.0F) +
        ")\n"
        "if ((arena.distance - 10).abs > 0.01 || arena.nx != 1) Fiber.abort(\"back wall\")");
    EXPECT_FALSE(HasFailure());
}

TEST_F(CarPhysics, ABallThrownIntoAGoalCornerFollowsTheCurve) {
    // A ball rising along the back of the left goal: the rounded corner turns it toward the field instead of stopping
    // it dead in a sharp corner.
    const float back = arena::LEFT - arena::GOAL_DEPTH;
    spawnBall(back + BALL_RADIUS + 1.0F, arena::GOAL_BOTTOM - BALL_RADIUS - 100.0F);
    throwBall(0.0F, -1500.0F);
    // The ball is in the goal: the match would score and take it out of play. Step its physics alone instead.
    wren(
        "var ball = Simulation.ball\n"
        "var furthest = ball.x\n"
        "for (i in 0...80) {\n"
        "    ball.step(1 / 120)\n"
        "    ball.constrain()\n"
        "    furthest = furthest.max(ball.x)\n"
        "}\n"
        "if (furthest <= " +
        std::to_string(back + BALL_RADIUS + 20.0F) + ") Fiber.abort(\"stuck in the corner at %(furthest)\")");
    EXPECT_FALSE(HasFailure());  // pushed out of the corner, toward the goal mouth
}

// --- Air control: the stick aims, the nose follows ---

namespace {

/** @brief Direction of the nose on screen, in radians (0 right, -pi/2 up), from a car transform. */
float noseAngle(const ecs::Transform& car) {
    const float facing = car.scaleX < 0.0F ? -1.0F : 1.0F;
    return std::atan2(facing * std::sin(car.rotation), facing * std::cos(car.rotation));
}

/** @brief Smallest angle between two directions, in radians. */
float angleBetween(const float a, const float b) {
    return std::fabs(std::remainder(a - b, 2.0F * std::numbers::pi_v<float>));
}

}  // namespace

TEST_F(CarPhysics, PushingUpInTheAirLiftsTheNoseAndBoostingClimbs) {
    spawnCar(arena::WIDTH / 2.0F, arena::BOTTOM - HALF_HEIGHT);
    run(0.3F);
    run(0.2F, 0.0F, true, false, 1.0F);  // jump, holding up
    run(0.4F, 0.0F, false, false, 1.0F);
    EXPECT_LT(angleBetween(noseAngle(transform()), -std::numbers::pi_v<float> / 2.0F), 0.25F);  // nose up
    const float start = transform().y;
    run(1.0F, 0.0F, false, true, 1.0F);                                   // boost, nose up
    EXPECT_LT(transform().y, start - 200.0F);                             // it climbs: 550 uu/s² against gravity
    EXPECT_LT(std::fabs(transform().x - (arena::WIDTH / 2.0F)), 100.0F);  // straight up
}

TEST_F(CarPhysics, TheNoseFollowsTheStickInEveryDirection) {
    constexpr float PI = std::numbers::pi_v<float>;
    struct Aim {
        float steer;
        float pitch;
        float angle;
    };
    for (const Aim aim : {Aim{.steer = 0.0F, .pitch = -1.0F, .angle = PI / 2.0F},     // down
                          Aim{.steer = 1.0F, .pitch = 1.0F, .angle = -PI / 4.0F},     // up and right
                          Aim{.steer = -1.0F, .pitch = 0.0F, .angle = PI},            // left: turns around
                          Aim{.steer = 0.0F, .pitch = 1.0F, .angle = -PI / 2.0F}}) {  // up
        spawnCar(arena::WIDTH / 2.0F, arena::TOP + 500.0F);                           // high in the air, falling
        run(1.0F, aim.steer, false, false, aim.pitch);
        EXPECT_LT(angleBetween(noseAngle(transform()), aim.angle), 0.1F)
            << "steer " << aim.steer << " pitch " << aim.pitch << ": nose at " << noseAngle(transform());
        _registry.killEntity(_car);
        _scripts.update(0.0F);
        _car = 0;
    }
}

TEST_F(CarPhysics, AirControlTurnsCalmlyAndStopsOnTheAim) {
    spawnCar(arena::WIDTH / 2.0F, arena::TOP + 400.0F);
    run(FRAME);
    float fastest = 0.0F;
    float previous = transform().rotation;
    for (int i = 0; i < 60; ++i) {
        run(FRAME, 0.0F, false, false, -1.0F);  // aim down: a quarter turn
        fastest = std::max(fastest, angleBetween(transform().rotation, previous) / FRAME);
        previous = transform().rotation;
    }
    EXPECT_LE(fastest, 4.2F + 0.05F);  // slower than Rocket League's 5.5 rad/s
    EXPECT_GT(fastest, 3.0F);
    // Arrived: it holds the aim instead of swinging past it.
    const float settled = noseAngle(transform());
    run(0.3F, 0.0F, false, false, -1.0F);
    EXPECT_LT(angleBetween(noseAngle(transform()), settled), 0.02F);
    EXPECT_LT(angleBetween(settled, std::numbers::pi_v<float> / 2.0F), 0.05F);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
