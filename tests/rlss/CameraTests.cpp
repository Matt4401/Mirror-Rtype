// Runs the client camera script (rlss/scripts/CameraDirector.wren) headless.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

#include "Arena.hpp"
#include "components/CameraComponent.hpp"
#include "components/ScriptComponent.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "game/Components.hpp"
#include "game/ScriptBindings.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace {

namespace arena = rlss::arena;
namespace game = rlss::game;

constexpr float FRAME = 1.0F / 60.0F;
constexpr float VIEW_WIDTH = 1280.0F;
constexpr float VIEW_HEIGHT = 720.0F;
constexpr float WALL = 40.0F;
constexpr float BOUNDS_LEFT = arena::LEFT - arena::GOAL_DEPTH - WALL;
constexpr float BOUNDS_RIGHT = arena::RIGHT + arena::GOAL_DEPTH + WALL;
constexpr float BOUNDS_TOP = arena::TOP - WALL;
constexpr float BOUNDS_BOTTOM = arena::BOTTOM + WALL;

class CameraDirector : public ::testing::Test {
  protected:
    void SetUp() override {
        std::filesystem::current_path(RLSS_PROJECT_DIR);
        _scripts.init(_registry, _bus);
        _scripts.enableHotReload(false);
        ASSERT_TRUE(game::installScriptBindings(_scripts));
        _car = spawnAt(arena::LEFT + 600.0F, arena::BOTTOM - 18.0F);
        _ball = spawnAt((arena::LEFT + arena::RIGHT) / 2.0F, arena::BOTTOM - 92.0F);
        _camera = _registry.spawnEntity();
        _registry.addComponent(_camera, ecs::CameraComponent{});
        _registry.addComponent(_camera, game::CameraRig{
                                            .car = static_cast<std::int64_t>(_car),
                                            .ball = static_cast<std::int64_t>(_ball),
                                            .viewWidth = VIEW_WIDTH,
                                            .viewHeight = VIEW_HEIGHT,
                                            .boundsLeft = BOUNDS_LEFT,
                                            .boundsTop = BOUNDS_TOP,
                                            .boundsRight = BOUNDS_RIGHT,
                                            .boundsBottom = BOUNDS_BOTTOM,
                                        });
        _registry.addComponent(_camera, ecs::ScriptComponent{
                                            .moduleName = std::string(game::CAMERA_SCRIPT_MODULE),
                                            .className = "CameraDirector",
                                            .hasUpdate = true,
                                        });
    }

    ecs::Entity spawnAt(const float x, const float y) {
        const ecs::Entity entity = _registry.spawnEntity();
        _registry.addComponent(entity, ecs::Transform{.x = x, .y = y});
        return entity;
    }

    void moveTo(const ecs::Entity entity, const float x, const float y) {
        auto& transform = _registry.getComponents<ecs::Transform>().get(entity);
        transform.x = x;
        transform.y = y;
    }

    void run(const float seconds) {
        for (float t = 0.0F; t < seconds; t += FRAME) {
            _scripts.update(FRAME);
            ASSERT_TRUE(viewInsideArena()) << "the camera shows past the arena";
        }
    }

    [[nodiscard]] const ecs::CameraComponent& camera() {
        return _registry.getComponents<ecs::CameraComponent>().get(_camera);
    }
    [[nodiscard]] float viewLeft() { return camera().targetX - (VIEW_WIDTH / 2.0F / camera().zoom); }
    [[nodiscard]] float viewRight() { return camera().targetX + (VIEW_WIDTH / 2.0F / camera().zoom); }
    [[nodiscard]] float viewTop() { return camera().targetY - (VIEW_HEIGHT / 2.0F / camera().zoom); }
    [[nodiscard]] float viewBottom() { return camera().targetY + (VIEW_HEIGHT / 2.0F / camera().zoom); }

    [[nodiscard]] bool viewInsideArena() {
        constexpr float TOLERANCE = 0.5F;
        return viewLeft() >= BOUNDS_LEFT - TOLERANCE && viewRight() <= BOUNDS_RIGHT + TOLERANCE &&
               viewTop() >= BOUNDS_TOP - TOLERANCE && viewBottom() <= BOUNDS_BOTTOM + TOLERANCE;
    }

    [[nodiscard]] const ecs::Transform& transformOf(const ecs::Entity entity) {
        return _registry.getComponents<ecs::Transform>().get(entity);
    }

    [[nodiscard]] bool onScreen(const ecs::Entity entity) {
        const auto& t = _registry.getComponents<ecs::Transform>().get(entity);
        return t.x >= viewLeft() && t.x <= viewRight() && t.y >= viewTop() && t.y <= viewBottom();
    }

    ecs::Registry _registry;
    ecs::EventBus _bus;
    systems::ScriptSystem _scripts;
    ecs::Entity _car = 0;
    ecs::Entity _ball = 0;
    ecs::Entity _camera = 0;
};

TEST_F(CameraDirector, FramesTheCarAndTheBallWithoutRotating) {
    run(3.0F);
    EXPECT_TRUE(onScreen(_car));
    EXPECT_TRUE(onScreen(_ball));
    EXPECT_FLOAT_EQ(camera().rotation, 0.0F);
    EXPECT_FLOAT_EQ(camera().offsetX, VIEW_WIDTH / 2.0F);
}

TEST_F(CameraDirector, ZoomsInWhenTheBallIsCloseAndOutWhenItIsFar) {
    moveTo(_ball, arena::LEFT + 900.0F, arena::BOTTOM - 92.0F);
    run(3.0F);
    const float close = camera().zoom;
    moveTo(_ball, arena::RIGHT - 600.0F, arena::TOP + 300.0F);
    run(3.0F);
    const float far = camera().zoom;
    EXPECT_GT(close, far * 1.3F);
    // Never closer than 1500 uu across, never wider than the arena.
    EXPECT_LE(close, VIEW_WIDTH / 1500.0F + 0.001F);
    EXPECT_GE(far,
              std::max(VIEW_WIDTH / (BOUNDS_RIGHT - BOUNDS_LEFT), VIEW_HEIGHT / (BOUNDS_BOTTOM - BOUNDS_TOP)) - 0.001F);
}

TEST_F(CameraDirector, AimsBetweenTheCarAndTheBallLeaningToTheBall) {
    const float carX = 1100.0F;
    const float ballX = 1700.0F;
    moveTo(_car, carX, arena::BOTTOM - 300.0F);
    moveTo(_ball, ballX, arena::BOTTOM - 300.0F);
    run(4.0F);
    EXPECT_GT(camera().targetX, carX + ((ballX - carX) * 0.35F));
    EXPECT_LT(camera().targetX, carX + ((ballX - carX) * 0.65F));
}

TEST_F(CameraDirector, EasesInsteadOfJumping) {
    run(3.0F);
    const float before = camera().targetX;
    const float zoomBefore = camera().zoom;
    moveTo(_ball, arena::RIGHT - 600.0F, arena::BOTTOM - 92.0F);
    run(FRAME);
    // One frame later the camera has only started to move.
    EXPECT_LT(std::fabs(camera().targetX - before), 60.0F);
    EXPECT_LT(std::fabs(camera().zoom - zoomBefore), zoomBefore * 0.05F);
    run(3.0F);
    EXPECT_GT(camera().targetX - before, 200.0F);
}

TEST_F(CameraDirector, StopsAgainstTheArenaNearAGoal) {
    moveTo(_car, arena::LEFT + 500.0F, arena::BOTTOM - 18.0F);
    moveTo(_ball, arena::LEFT - 120.0F, arena::GOAL_BOTTOM - 92.0F);  // in the left goal
    run(3.0F);
    EXPECT_NEAR(viewLeft(), BOUNDS_LEFT, 1.0F);
    EXPECT_TRUE(onScreen(_ball));
}

TEST_F(CameraDirector, ShowsTheWholeCenterAtKickoff) {
    _registry.getComponents<game::CameraRig>().get(_camera).phase = 2;
    run(4.0F);
    EXPECT_NEAR(camera().targetX, (arena::LEFT + arena::RIGHT) / 2.0F, 5.0F);
    EXPECT_NEAR(camera().zoom,
                std::max(VIEW_WIDTH / (BOUNDS_RIGHT - BOUNDS_LEFT), VIEW_HEIGHT / (BOUNDS_BOTTOM - BOUNDS_TOP)), 0.01F);
}

TEST_F(CameraDirector, FollowsAHighBallUpWhenTheCarStaysDown) {
    moveTo(_ball, arena::LEFT + 800.0F, arena::TOP + 150.0F);
    run(3.0F);
    EXPECT_TRUE(onScreen(_ball));
    EXPECT_TRUE(onScreen(_car));
    EXPECT_LT(viewTop(), arena::TOP + 100.0F);
}

TEST_F(CameraDirector, KeepsTheCarOnScreenWhenTheBallFliesFarAway) {
    // Car and ball close together: zoomed in.
    moveTo(_car, arena::LEFT + 900.0F, arena::BOTTOM - 18.0F);
    moveTo(_ball, arena::LEFT + 1200.0F, arena::BOTTOM - 92.0F);
    run(3.0F);
    // The ball is cleared to the other end, high: the aim heads there faster than the zoom pulls back.
    moveTo(_ball, arena::RIGHT - 300.0F, arena::TOP + 200.0F);
    for (int i = 0; i < 240; ++i) {
        run(FRAME);
        ASSERT_GT(transformOf(_car).x - viewLeft(), 150.0F) << "frame " << i;
        ASSERT_TRUE(onScreen(_car)) << "frame " << i;
    }
    // Shows as much toward the ball as possible: the car is on the left part of the screen.
    EXPECT_LT(transformOf(_car).x, camera().targetX);
}

TEST_F(CameraDirector, AFastCarNeverLeavesTheScreen) {
    moveTo(_ball, arena::RIGHT - 300.0F, arena::BOTTOM - 92.0F);
    run(3.0F);
    // Supersonic car (2300 uu/s) driving away from the ball, back and forth along the floor.
    float x = arena::RIGHT - 900.0F;
    float direction = -1.0F;
    for (int i = 0; i < 600; ++i) {
        x += direction * 2300.0F * FRAME;
        if (x < arena::LEFT + 500.0F || x > arena::RIGHT - 500.0F) {
            direction = -direction;
        }
        moveTo(_car, x, arena::BOTTOM - 18.0F);
        run(FRAME);
        const float fromEdge = std::min(transformOf(_car).x - viewLeft(), viewRight() - transformOf(_car).x);
        ASSERT_GE(fromEdge, 150.0F) << "frame " << i;
    }
}

}  // namespace

TEST_F(CameraDirector, FollowsACarWhoseEntitySlotWasReused) {
    // Entities are generational: a slot freed and reused gets a new generation in the high bits of the handle. The rig
    // must carry the whole handle to the script, or the camera would look for a dead entity and lose the car.
    _registry.killEntity(_car);
    _car = spawnAt(arena::RIGHT - 600.0F, arena::BOTTOM - 18.0F);
    ASSERT_GT(ecs::entityGeneration(_car), 0U);
    _registry.getComponents<game::CameraRig>().get(_camera).car = static_cast<std::int64_t>(_car);
    run(3.0F);
    EXPECT_TRUE(onScreen(_car));
    EXPECT_GT(camera().targetX, (arena::LEFT + arena::RIGHT) / 2.0F);  // over to the right, where the new car is
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
