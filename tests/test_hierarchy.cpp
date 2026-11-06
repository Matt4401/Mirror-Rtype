#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <string_view>
#include <vector>

#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "hierarchy/HierarchyComponents.hpp"
#include "hierarchy/HierarchyHooks.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "hierarchy/TransformMath.hpp"
#include "hierarchy/TransformPropagation.hpp"

namespace {

constexpr float EPS = 1e-4F;

/** @brief Registry with the hierarchy hooks installed, as HierarchyPlugin does. */
struct HierarchyTest : ::testing::Test {
    ecs::Registry registry;
    hierarchy::HierarchyHooks hooks{registry};

    ecs::Entity spawnAt(float x, float y, float rotation = 0.0F, float scaleX = 1.0F, float scaleY = 1.0F) {
        const ecs::Entity e = registry.spawnEntity();
        registry.addComponent<ecs::Transform>(
            e, ecs::Transform{.x = x, .y = y, .z = 0.0F, .rotation = rotation, .scaleX = scaleX, .scaleY = scaleY});
        return e;
    }

    ecs::Transform& world(ecs::Entity e) { return registry.getComponents<ecs::Transform>().get(e); }

    hierarchy::LocalTransform& local(ecs::Entity e) {
        return registry.getComponents<hierarchy::LocalTransform>().get(e);
    }

    std::vector<ecs::Entity> children(ecs::Entity e) {
        const auto span = hierarchy::childrenOf(registry, e);
        return {span.begin(), span.end()};
    }
};

}  // namespace

// --- Links -------------------------------------------------------------------------------------------------------

TEST_F(HierarchyTest, SetParentLinksBothSidesInOrder) {
    const ecs::Entity parent = spawnAt(0.0F, 0.0F);
    const ecs::Entity a = spawnAt(1.0F, 0.0F);
    const ecs::Entity b = spawnAt(2.0F, 0.0F);
    const ecs::Entity c = spawnAt(3.0F, 0.0F);

    EXPECT_TRUE(hierarchy::setParent(registry, a, parent));
    EXPECT_TRUE(hierarchy::setParent(registry, b, parent));
    EXPECT_TRUE(hierarchy::setParent(registry, c, parent));

    EXPECT_EQ(children(parent), (std::vector{a, b, c}));
    EXPECT_EQ(hierarchy::parentOf(registry, a), parent);
    EXPECT_FALSE(hierarchy::parentOf(registry, parent).has_value());
    EXPECT_TRUE(registry.hasComponent<hierarchy::LocalTransform>(a));

    // Setting the same parent again is a no-op that succeeds and keeps the order.
    EXPECT_TRUE(hierarchy::setParent(registry, a, parent));
    EXPECT_EQ(children(parent), (std::vector{a, b, c}));
}

TEST_F(HierarchyTest, ReparentMovesChildBetweenLists) {
    const ecs::Entity first = spawnAt(0.0F, 0.0F);
    const ecs::Entity second = spawnAt(10.0F, 0.0F);
    const ecs::Entity child = spawnAt(5.0F, 0.0F);

    ASSERT_TRUE(hierarchy::setParent(registry, child, first));
    ASSERT_TRUE(hierarchy::setParent(registry, child, second));

    EXPECT_TRUE(children(first).empty());
    EXPECT_FALSE(registry.hasComponent<hierarchy::Children>(first));
    EXPECT_EQ(children(second), (std::vector{child}));
    EXPECT_EQ(hierarchy::parentOf(registry, child), second);
    EXPECT_NEAR(local(child).x, -5.0F, EPS);
}

TEST_F(HierarchyTest, RemoveParentDetachesAndKeepsWorld) {
    const ecs::Entity parent = spawnAt(3.0F, 4.0F);
    const ecs::Entity child = spawnAt(5.0F, 4.0F);
    ASSERT_TRUE(hierarchy::setParent(registry, child, parent));

    hierarchy::removeParent(registry, child);

    EXPECT_FALSE(hierarchy::parentOf(registry, child).has_value());
    EXPECT_FALSE(registry.hasComponent<hierarchy::LocalTransform>(child));
    EXPECT_FALSE(registry.hasComponent<hierarchy::Children>(parent));
    EXPECT_NEAR(world(child).x, 5.0F, EPS);

    hierarchy::removeParent(registry, child);  // no parent: no-op
}

TEST_F(HierarchyTest, AncestorAndRootQueries) {
    const ecs::Entity a = spawnAt(0.0F, 0.0F);
    const ecs::Entity b = spawnAt(0.0F, 0.0F);
    const ecs::Entity c = spawnAt(0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(registry, b, a));
    ASSERT_TRUE(hierarchy::setParent(registry, c, b));

    EXPECT_TRUE(hierarchy::isAncestorOf(registry, a, c));
    EXPECT_TRUE(hierarchy::isAncestorOf(registry, b, c));
    EXPECT_FALSE(hierarchy::isAncestorOf(registry, c, a));
    EXPECT_FALSE(hierarchy::isAncestorOf(registry, c, c));
    EXPECT_EQ(hierarchy::rootOf(registry, c), a);
    EXPECT_EQ(hierarchy::rootOf(registry, a), a);
}

// --- Rejection ---------------------------------------------------------------------------------------------------

TEST_F(HierarchyTest, RejectsSelfCyclesAndDeadEntities) {
    const ecs::Entity a = spawnAt(0.0F, 0.0F);
    const ecs::Entity b = spawnAt(0.0F, 0.0F);
    const ecs::Entity c = spawnAt(0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(registry, b, a));
    ASSERT_TRUE(hierarchy::setParent(registry, c, b));

    EXPECT_FALSE(hierarchy::setParent(registry, a, a));
    EXPECT_FALSE(hierarchy::setParent(registry, a, c));  // a -> b -> c -> a would be a cycle
    EXPECT_FALSE(hierarchy::parentOf(registry, a).has_value());
    EXPECT_EQ(children(c), std::vector<ecs::Entity>{});

    const ecs::Entity dead = registry.spawnEntity();
    registry.killEntity(dead);
    EXPECT_FALSE(hierarchy::setParent(registry, dead, a));
    EXPECT_FALSE(hierarchy::setParent(registry, a, dead));

    // Stale handle: same slot, newer generation.
    const ecs::Entity recycled = registry.spawnEntity();
    ASSERT_EQ(ecs::entityIndex(recycled), ecs::entityIndex(dead));
    EXPECT_FALSE(hierarchy::setParent(registry, recycled, dead));
    EXPECT_FALSE(hierarchy::setParent(registry, dead, recycled));
    EXPECT_EQ(children(a), (std::vector{b}));
}

// --- Cascade -----------------------------------------------------------------------------------------------------

TEST_F(HierarchyTest, KillingRootKillsAllDescendants) {
    const ecs::Entity root = spawnAt(0.0F, 0.0F);
    const ecs::Entity mid = spawnAt(0.0F, 0.0F);
    const ecs::Entity leafA = spawnAt(0.0F, 0.0F);
    const ecs::Entity leafB = spawnAt(0.0F, 0.0F);
    const ecs::Entity unrelated = spawnAt(0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(registry, mid, root));
    ASSERT_TRUE(hierarchy::setParent(registry, leafA, mid));
    ASSERT_TRUE(hierarchy::setParent(registry, leafB, mid));

    registry.killEntity(root);

    for (const ecs::Entity e : {root, mid, leafA, leafB}) {
        EXPECT_FALSE(registry.isAlive(e));
        EXPECT_FALSE(registry.hasComponent<hierarchy::Parent>(e));
        EXPECT_FALSE(registry.hasComponent<hierarchy::Children>(e));
    }
    EXPECT_TRUE(registry.isAlive(unrelated));
}

TEST_F(HierarchyTest, KillingMiddleNodeUnlinksItFromParent) {
    const ecs::Entity root = spawnAt(0.0F, 0.0F);
    const ecs::Entity first = spawnAt(0.0F, 0.0F);
    const ecs::Entity mid = spawnAt(0.0F, 0.0F);
    const ecs::Entity last = spawnAt(0.0F, 0.0F);
    const ecs::Entity leaf = spawnAt(0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(registry, first, root));
    ASSERT_TRUE(hierarchy::setParent(registry, mid, root));
    ASSERT_TRUE(hierarchy::setParent(registry, last, root));
    ASSERT_TRUE(hierarchy::setParent(registry, leaf, mid));

    registry.killEntity(mid);

    EXPECT_TRUE(registry.isAlive(root));
    EXPECT_FALSE(registry.isAlive(leaf));
    EXPECT_EQ(children(root), (std::vector{first, last}));
}

TEST_F(HierarchyTest, DeepChainCascadesWithoutStackOverflow) {
    constexpr int DEPTH = 10'000;
    const ecs::Entity root = spawnAt(0.0F, 0.0F);
    ecs::Entity previous = root;
    std::vector<ecs::Entity> chain;
    chain.reserve(DEPTH);
    for (int i = 0; i < DEPTH; ++i) {
        const ecs::Entity e = spawnAt(0.0F, 0.0F);
        ASSERT_TRUE(hierarchy::setParent(registry, e, previous));
        chain.push_back(e);
        previous = e;
    }

    registry.killEntity(root);

    EXPECT_FALSE(registry.isAlive(chain.front()));
    EXPECT_FALSE(registry.isAlive(chain.back()));
    EXPECT_TRUE(registry.getComponents<hierarchy::Parent>().getDense().empty());
}

// --- Manual component removal ------------------------------------------------------------------------------------

TEST_F(HierarchyTest, RemovingParentComponentByHandUnlinksChild) {
    const ecs::Entity parent = spawnAt(0.0F, 0.0F);
    const ecs::Entity a = spawnAt(0.0F, 0.0F);
    const ecs::Entity b = spawnAt(0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(registry, a, parent));
    ASSERT_TRUE(hierarchy::setParent(registry, b, parent));

    registry.removeComponent<hierarchy::Parent>(a);

    EXPECT_EQ(children(parent), (std::vector{b}));
}

TEST_F(HierarchyTest, RemovingChildrenComponentByHandDetachesChildren) {
    const ecs::Entity parent = spawnAt(0.0F, 0.0F);
    const ecs::Entity a = spawnAt(0.0F, 0.0F);
    const ecs::Entity b = spawnAt(0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(registry, a, parent));
    ASSERT_TRUE(hierarchy::setParent(registry, b, parent));

    registry.removeComponent<hierarchy::Children>(parent);

    for (const ecs::Entity e : {a, b}) {
        EXPECT_FALSE(hierarchy::parentOf(registry, e).has_value());
        EXPECT_FALSE(registry.hasComponent<hierarchy::LocalTransform>(e));
    }
    registry.killEntity(parent);
    EXPECT_TRUE(registry.isAlive(a));
    EXPECT_TRUE(registry.isAlive(b));
}

// --- Math --------------------------------------------------------------------------------------------------------

TEST(HierarchyMath, ComposeTranslation) {
    const ecs::Transform parent{.x = 10.0F, .y = 5.0F, .z = 1.0F};
    const ecs::Transform w = hierarchy::compose(parent, hierarchy::LocalTransform{.x = 2.0F, .y = 3.0F, .z = 0.5F});
    EXPECT_NEAR(w.x, 12.0F, EPS);
    EXPECT_NEAR(w.y, 8.0F, EPS);
    EXPECT_NEAR(w.z, 1.5F, EPS);
}

TEST(HierarchyMath, ComposeRotationOrbitsAroundParent) {
    const ecs::Transform parent{.x = 10.0F, .y = 10.0F, .rotation = std::numbers::pi_v<float> / 2.0F};
    const ecs::Transform w =
        hierarchy::compose(parent, hierarchy::LocalTransform{.x = 1.0F, .y = 0.0F, .rotation = 0.25F});
    EXPECT_NEAR(w.x, 10.0F, EPS);
    EXPECT_NEAR(w.y, 11.0F, EPS);
    EXPECT_NEAR(w.rotation, (std::numbers::pi_v<float> / 2.0F) + 0.25F, EPS);
}

TEST(HierarchyMath, ComposeScale) {
    const ecs::Transform parent{.scaleX = 2.0F, .scaleY = 3.0F};
    const ecs::Transform w =
        hierarchy::compose(parent, hierarchy::LocalTransform{.x = 1.0F, .y = 1.0F, .scaleX = 0.5F, .scaleY = 2.0F});
    EXPECT_NEAR(w.x, 2.0F, EPS);
    EXPECT_NEAR(w.y, 3.0F, EPS);
    EXPECT_NEAR(w.scaleX, 1.0F, EPS);
    EXPECT_NEAR(w.scaleY, 6.0F, EPS);
}

TEST(HierarchyMath, ToLocalIsInverseOfCompose) {
    const ecs::Transform parent{.x = -3.0F, .y = 7.0F, .z = 2.0F, .rotation = 1.2F, .scaleX = 1.5F, .scaleY = 0.5F};
    const hierarchy::LocalTransform local{
        .x = 4.0F, .y = -2.0F, .z = 1.0F, .rotation = -0.4F, .scaleX = 2.0F, .scaleY = 3.0F};
    const hierarchy::LocalTransform back = hierarchy::toLocal(parent, hierarchy::compose(parent, local));
    EXPECT_NEAR(back.x, local.x, EPS);
    EXPECT_NEAR(back.y, local.y, EPS);
    EXPECT_NEAR(back.z, local.z, EPS);
    EXPECT_NEAR(back.rotation, local.rotation, EPS);
    EXPECT_NEAR(back.scaleX, local.scaleX, EPS);
    EXPECT_NEAR(back.scaleY, local.scaleY, EPS);
}

TEST(HierarchyMath, ZeroParentScaleNeverProducesNaN) {
    const ecs::Transform parent{.x = 1.0F, .y = 1.0F, .scaleX = 0.0F, .scaleY = 0.0F};
    const hierarchy::LocalTransform l = hierarchy::toLocal(parent, ecs::Transform{.x = 5.0F, .y = 5.0F});
    for (const float v : {l.x, l.y, l.z, l.rotation, l.scaleX, l.scaleY}) {
        EXPECT_TRUE(std::isfinite(v));
    }
    EXPECT_EQ(l.x, 0.0F);
    EXPECT_EQ(l.scaleX, 1.0F);
}

// --- Propagation -------------------------------------------------------------------------------------------------

TEST_F(HierarchyTest, SetParentKeepsWorldPosition) {
    const ecs::Entity parent = spawnAt(10.0F, 0.0F, 0.7F, 2.0F, 2.0F);
    const ecs::Entity child = spawnAt(3.0F, -4.0F, 0.2F);
    ASSERT_TRUE(hierarchy::setParent(registry, child, parent));

    hierarchy::propagateTransforms(registry);

    EXPECT_NEAR(world(child).x, 3.0F, EPS);
    EXPECT_NEAR(world(child).y, -4.0F, EPS);
    EXPECT_NEAR(world(child).rotation, 0.2F, EPS);
    EXPECT_NEAR(world(child).scaleX, 1.0F, EPS);
}

TEST_F(HierarchyTest, PropagatesThroughThreeLevelsInOnePass) {
    const ecs::Entity root = spawnAt(0.0F, 0.0F);
    const ecs::Entity mid = spawnAt(0.0F, 0.0F);
    const ecs::Entity leaf = spawnAt(0.0F, 0.0F);
    ASSERT_TRUE(hierarchy::setParent(registry, mid, root));
    ASSERT_TRUE(hierarchy::setParent(registry, leaf, mid));
    local(mid) = hierarchy::LocalTransform{.x = 10.0F, .rotation = std::numbers::pi_v<float> / 2.0F};
    local(leaf) = hierarchy::LocalTransform{.x = 1.0F};

    world(root).x = 100.0F;
    hierarchy::propagateTransforms(registry);

    EXPECT_NEAR(world(mid).x, 110.0F, EPS);
    EXPECT_NEAR(world(mid).y, 0.0F, EPS);
    // mid is rotated by 90 degrees: leaf's local +x points along world +y.
    EXPECT_NEAR(world(leaf).x, 110.0F, EPS);
    EXPECT_NEAR(world(leaf).y, 1.0F, EPS);
}

TEST_F(HierarchyTest, ChildWithoutLocalTransformIsLeftAloneButItsChildrenFollowIt) {
    const ecs::Entity root = spawnAt(0.0F, 0.0F);
    const ecs::Entity mid = spawnAt(5.0F, 5.0F);
    const ecs::Entity leaf = spawnAt(6.0F, 5.0F);
    ASSERT_TRUE(hierarchy::setParent(registry, mid, root));
    ASSERT_TRUE(hierarchy::setParent(registry, leaf, mid));
    registry.removeComponent<hierarchy::LocalTransform>(mid);

    world(root).x = 100.0F;
    world(mid).y = 50.0F;
    hierarchy::propagateTransforms(registry);

    EXPECT_NEAR(world(mid).x, 5.0F, EPS);
    EXPECT_NEAR(world(mid).y, 50.0F, EPS);
    EXPECT_NEAR(world(leaf).x, 6.0F, EPS);
    EXPECT_NEAR(world(leaf).y, 50.0F, EPS);
}

TEST_F(HierarchyTest, PropagationCreatesMissingWorldTransform) {
    const ecs::Entity parent = spawnAt(4.0F, 2.0F);
    const ecs::Entity child = registry.spawnEntity();
    ASSERT_TRUE(hierarchy::setParent(registry, child, parent));
    local(child) = hierarchy::LocalTransform{.x = 1.0F};

    hierarchy::propagateTransforms(registry);

    ASSERT_TRUE(registry.hasComponent<ecs::Transform>(child));
    EXPECT_NEAR(world(child).x, 5.0F, EPS);
    EXPECT_NEAR(world(child).y, 2.0F, EPS);
}

TEST(HierarchyPropagation, EmptyRegistryIsNoOp) {
    ecs::Registry registry;
    hierarchy::propagateTransforms(registry);
    EXPECT_TRUE(registry.getComponents<hierarchy::Children>().getDense().empty());
}

// --- Plugin ------------------------------------------------------------------------------------------------------

namespace {

/** @brief Moves the parent in `update`, before HierarchyPlugin's lateUpdate. */
class MoverPlugin final : public engine::IPlugin {
  public:
    explicit MoverPlugin(ecs::Entity& target) : _target(target) {}

    [[nodiscard]] std::string_view name() const noexcept override { return "MoverPlugin"; }

    void update(engine::Engine& engine, const ecs::time::Time& /*time*/) override {
        engine.registry().getComponents<ecs::Transform>().get(_target).x += 10.0F;
    }

  private:
    ecs::Entity& _target;
};

}  // namespace

TEST(HierarchyPlugin, ChildFollowsParentMovedInUpdateWithinSameFrame) {
    ecs::Entity parent = ecs::NULL_ENTITY;
    engine::Engine engineInstance;
    engineInstance.addPlugin<hierarchy::HierarchyPlugin>().addPlugin<MoverPlugin>(parent);

    auto& registry = engineInstance.registry();
    parent = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(parent, ecs::Transform{});
    const ecs::Entity child = registry.spawnEntity();
    registry.addComponent<ecs::Transform>(child, ecs::Transform{.x = 1.0F});
    ASSERT_TRUE(hierarchy::setParent(registry, child, parent));

    engineInstance.step(0.016F);

    EXPECT_NEAR(registry.getComponents<ecs::Transform>().get(parent).x, 10.0F, EPS);
    EXPECT_NEAR(registry.getComponents<ecs::Transform>().get(child).x, 11.0F, EPS);

    // The plugin's hooks are installed: destroying the parent cascades.
    registry.killEntity(parent);
    EXPECT_FALSE(registry.isAlive(child));
    engineInstance.shutdown();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
