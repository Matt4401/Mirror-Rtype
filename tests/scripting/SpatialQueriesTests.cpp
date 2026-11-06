#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "components/ScriptComponent.hpp"
#include "components/Tag.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/SpatialQueries.hpp"
#include "events/EventBus.hpp"
#include "scripting/ComponentBinder.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace {

/** @brief Spawns an entity with a Transform at (x, y) and, if not empty, a Tag. */
ecs::Entity spawnAt(ecs::Registry& registry, float x, float y, std::string const& tag = {}) {
    ecs::Entity const e = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(e, ecs::Transform{.x = x, .y = y});
    if (!tag.empty()) {
        registry.addComponent<ecs::Tag>(e, ecs::Tag{.name = tag});
    }
    return e;
}

/** @brief Registry with Tag, Transform and ScriptComponent registered. */
class SpatialQueries : public ::testing::Test {
  protected:
    void SetUp() override {
        registry.registerComponent<ecs::Tag>();
        registry.registerComponent<ecs::Transform>();
        registry.registerComponent<ecs::ScriptComponent>();
    }

    ecs::Registry registry;
    std::vector<ecs::Entity> out;
};

/** @brief SpatialQueries fixture with a ScriptSystem and a `Probe` Wren class calling `World`. */
class WorldScripting : public SpatialQueries {
  protected:
    void SetUp() override {
        SpatialQueries::SetUp();
        scripts.init(registry, bus);
        ASSERT_TRUE(scripts.getVm().interpret("probe", R"wren(
import "core/World" for World
class Probe {
    static tagged(tag) { World.getEntitiesWithTag(tag).map {|e| e.id }.toList }
    static nearest(x, y, tag) {
        var e = World.getNearestWithTag(x, y, tag)
        return e == null ? null : e.id
    }
    static around(x, y, r) { World.getEntitiesInRadius(x, y, r).map {|e| e.id }.toList }
}
)wren"));
    }

    /** @brief Calls a static `Probe` method; the result is left in slot 0. */
    WrenInterpretResult callProbe(std::string const& signature, auto... args) {
        auto& vm = scripts.getVm();
        auto handle = vm.makeCallHandle(signature);
        vm.ensureSlots(static_cast<int>(sizeof...(args)) + 1);
        vm.getVariable("probe", "Probe", 0);
        int slot = 1;
        (setArg(slot++, args), ...);
        return vm.call(handle);
    }

    /** @brief Reads the list of numbers in slot 0 as entity ids. */
    std::vector<ecs::Entity> resultIds() {
        WrenVM* raw = scripts.getVm().raw();
        std::vector<ecs::Entity> ids;
        int const count = wrenGetListCount(raw, 0);
        wrenEnsureSlots(raw, 2);
        for (int i = 0; i < count; ++i) {
            wrenGetListElement(raw, 0, i, 1);
            ids.push_back(static_cast<ecs::Entity>(wrenGetSlotDouble(raw, 1)));
        }
        return ids;
    }

    ecs::EventBus bus;
    systems::ScriptSystem scripts;

  private:
    void setArg(int slot, double value) { wrenSetSlotDouble(scripts.getVm().raw(), slot, value); }
    void setArg(int slot, char const* value) { wrenSetSlotString(scripts.getVm().raw(), slot, value); }
};

}  // namespace

// --- C++ API ---------------------------------------------------------------------------------------------------

TEST_F(SpatialQueries, EntitiesWithTagFiltersExactly) {
    ecs::Entity const a = spawnAt(registry, 0, 0, "enemy");
    spawnAt(registry, 0, 0, "player");
    spawnAt(registry, 0, 0, "Enemy");
    ecs::Entity const b = spawnAt(registry, 5, 5, "enemy");

    ecs::entitiesWithTag(registry, "enemy", out);
    EXPECT_EQ(out, (std::vector<ecs::Entity>{a, b}));

    ecs::entitiesWithTag(registry, "boss", out);
    EXPECT_TRUE(out.empty());
}

TEST_F(SpatialQueries, NearestWithTagPicksClosestMatchWithTransform) {
    spawnAt(registry, 1, 0, "player");
    ecs::Entity const far = spawnAt(registry, 100, 0, "enemy");
    ecs::Entity const near = spawnAt(registry, -10, 0, "enemy");
    ecs::Entity const noTransform = registry.spawnEntity();
    registry.addComponent<ecs::Tag>(noTransform, ecs::Tag{.name = "enemy"});

    EXPECT_EQ(ecs::nearestWithTag(registry, 0, 0, "enemy"), near);
    EXPECT_EQ(ecs::nearestWithTag(registry, 90, 0, "enemy"), far);
    EXPECT_EQ(ecs::nearestWithTag(registry, 0, 0, "boss"), std::nullopt);
}

TEST_F(SpatialQueries, NearestWithTagSkipsExcludedEntities) {
    ecs::Entity const near = spawnAt(registry, 1, 0, "enemy");
    ecs::Entity const far = spawnAt(registry, 50, 0, "enemy");
    std::vector<ecs::Entity> const excluded{near};

    EXPECT_EQ(ecs::nearestWithTag(registry, 0, 0, "enemy", excluded), far);
}

TEST_F(SpatialQueries, NearestWithTagRejectsNonFinitePoint) {
    spawnAt(registry, 0, 0, "enemy");
    EXPECT_EQ(ecs::nearestWithTag(registry, std::numeric_limits<float>::quiet_NaN(), 0, "enemy"), std::nullopt);
}

TEST_F(SpatialQueries, EntitiesInRadiusIncludesBoundary) {
    ecs::Entity const inside = spawnAt(registry, 3, 4);  // distance 5
    ecs::Entity const tagged = spawnAt(registry, -2, 0, "enemy");
    spawnAt(registry, 6, 0);

    ecs::entitiesInRadius(registry, 0, 0, 5, out);
    EXPECT_EQ(out, (std::vector<ecs::Entity>{inside, tagged}));
}

TEST_F(SpatialQueries, EntitiesInRadiusBoundsTheRadius) {
    spawnAt(registry, 0, 0);
    ecs::Entity const veryFar = spawnAt(registry, ecs::MAX_QUERY_RADIUS * 2, 0);

    ecs::entitiesInRadius(registry, 0, 0, -1, out);
    EXPECT_TRUE(out.empty());
    ecs::entitiesInRadius(registry, 0, 0, std::numeric_limits<float>::infinity(), out);
    EXPECT_TRUE(out.empty());

    ecs::entitiesInRadius(registry, 0, 0, std::numeric_limits<float>::max(), out);
    EXPECT_EQ(out.size(), 1U);
    EXPECT_NE(out.front(), veryFar);
}

TEST(SpatialQueriesUnregistered, ReturnEmptyWithoutThrowing) {
    ecs::Registry const registry;
    std::vector<ecs::Entity> out{42};

    EXPECT_NO_THROW(ecs::entitiesWithTag(registry, "enemy", out));
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(ecs::nearestWithTag(registry, 0, 0, "enemy"), std::nullopt);
    out.push_back(1);
    EXPECT_NO_THROW(ecs::entitiesInRadius(registry, 0, 0, 10, out));
    EXPECT_TRUE(out.empty());
}

// --- Wren core/World ------------------------------------------------------------------------------------------

TEST_F(WorldScripting, GetEntitiesWithTagReturnsEntityWrappers) {
    ecs::Entity const a = spawnAt(registry, 0, 0, "enemy");
    spawnAt(registry, 0, 0, "player");
    ecs::Entity const b = spawnAt(registry, 1, 1, "enemy");

    ASSERT_EQ(callProbe("tagged(_)", "enemy"), WREN_RESULT_SUCCESS);
    EXPECT_EQ(resultIds(), (std::vector<ecs::Entity>{a, b}));

    ASSERT_EQ(callProbe("tagged(_)", "boss"), WREN_RESULT_SUCCESS);
    EXPECT_TRUE(resultIds().empty());
}

TEST_F(WorldScripting, GetNearestWithTagReturnsEntityOrNull) {
    spawnAt(registry, 100, 0, "player");
    ecs::Entity const near = spawnAt(registry, 10, 0, "player");

    ASSERT_EQ(callProbe("nearest(_,_,_)", 0.0, 0.0, "player"), WREN_RESULT_SUCCESS);
    EXPECT_EQ(scripts.getVm().getSlot<double>(0), static_cast<double>(near));

    ASSERT_EQ(callProbe("nearest(_,_,_)", 0.0, 0.0, "boss"), WREN_RESULT_SUCCESS);
    EXPECT_EQ(wrenGetSlotType(scripts.getVm().raw(), 0), WREN_TYPE_NULL);
}

TEST_F(WorldScripting, GetEntitiesInRadiusForAreaOfEffect) {
    ecs::Entity const hit1 = spawnAt(registry, 10, 0, "enemy");
    ecs::Entity const hit2 = spawnAt(registry, 0, -20, "enemy");
    spawnAt(registry, 200, 0, "enemy");

    ASSERT_EQ(callProbe("around(_,_,_)", 0.0, 0.0, 25.0), WREN_RESULT_SUCCESS);
    EXPECT_EQ(resultIds(), (std::vector<ecs::Entity>{hit1, hit2}));

    ASSERT_EQ(callProbe("around(_,_,_)", 0.0, 0.0, -5.0), WREN_RESULT_SUCCESS);
    EXPECT_TRUE(resultIds().empty());
}

TEST_F(WorldScripting, QueriesSkipEntitiesPendingDestroy) {
    ecs::Entity const doomed = spawnAt(registry, 1, 0, "enemy");
    ecs::Entity const survivor = spawnAt(registry, 50, 0, "enemy");
    scripts.queueDestroyEntity(doomed);

    ASSERT_EQ(callProbe("tagged(_)", "enemy"), WREN_RESULT_SUCCESS);
    EXPECT_EQ(resultIds(), (std::vector<ecs::Entity>{survivor}));

    ASSERT_EQ(callProbe("nearest(_,_,_)", 0.0, 0.0, "enemy"), WREN_RESULT_SUCCESS);
    EXPECT_EQ(scripts.getVm().getSlot<double>(0), static_cast<double>(survivor));

    ASSERT_EQ(callProbe("around(_,_,_)", 0.0, 0.0, 100.0), WREN_RESULT_SUCCESS);
    EXPECT_EQ(resultIds(), (std::vector<ecs::Entity>{survivor}));
}

TEST_F(WorldScripting, InvalidArgumentsAbortTheFiberWithoutCrashing) {
    ASSERT_EQ(callProbe("tagged(_)", 3.0), WREN_RESULT_RUNTIME_ERROR);
    ASSERT_EQ(callProbe("nearest(_,_,_)", "x", 0.0, "enemy"), WREN_RESULT_RUNTIME_ERROR);
}

TEST_F(WorldScripting, HomingMissileSteersTowardNearestEnemy) {
    scripting::ComponentBinder<ecs::Transform> binder(scripts.getVm(), "core/Transform", "Transform");
    binder.property<&ecs::Transform::x>("x");
    binder.property<&ecs::Transform::y>("y");
    ASSERT_TRUE(scripts.getVm().interpret("core/Transform", binder.generateWrenClass()));
    ASSERT_TRUE(scripts.getVm().interpret("homing", R"wren(
import "core/Transform" for Transform
import "core/World" for World
class HomingMissile {
    construct new(entity) { _pos = Transform.new(entity.id) }
    update(dt) {
        var target = World.getNearestWithTag(_pos.x, _pos.y, "enemy")
        if (target == null) return
        var goal = Transform.new(target.id)
        var dx = goal.x - _pos.x
        var dy = goal.y - _pos.y
        var len = (dx * dx + dy * dy).sqrt
        if (len < 1) return
        _pos.x = _pos.x + dx / len * 300 * dt
        _pos.y = _pos.y + dy / len * 300 * dt
    }
}
)wren"));

    ecs::Entity const closer = spawnAt(registry, 100, 100, "enemy");
    ecs::Entity const farther = spawnAt(registry, -400, 0, "enemy");
    ecs::Entity const missile = spawnAt(registry, 0, 0, "missile");
    registry.addComponent<ecs::ScriptComponent>(
        missile, ecs::ScriptComponent{
                     .moduleName = "homing", .className = "HomingMissile", .hasUpdate = true, .initialized = false});

    auto distanceTo = [&](ecs::Entity target) {
        auto const& m = registry.getComponents<ecs::Transform>().get(missile);
        auto const& t = registry.getComponents<ecs::Transform>().get(target);
        return std::hypot(t.x - m.x, t.y - m.y);
    };
    float const startCloser = distanceTo(closer);
    float const startFarther = distanceTo(farther);

    for (int i = 0; i < 10; ++i) {
        scripts.update(0.016F);
    }

    EXPECT_LT(distanceTo(closer), startCloser - 30.0F);
    EXPECT_GT(distanceTo(farther), startFarther);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
