#include <gtest/gtest.h>

#include <string>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "collision/CollisionRouter.hpp"
#include "collision/GameCollisionEvents.hpp"
#include "components/Renderable.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "gameplay/ShotHitRule.hpp"
#include "prefabs/ClientBullet.hpp"
#include "prefabs/ClientEnemy.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"
#include "systems/physics/PhysicsSystem.hpp"

namespace {

using client::gameplay::ShotHitRule;
using rtype::server::collision::CollisionRouter;
using rtype::server::collision::ShotHitEvent;

constexpr float STEP = 1.0F / 60.0F;

/** @brief Physics world, router and rule wired on one registry and bus, as in RTypeGamePlugin. */
class ShotHitRuleTest : public ::testing::Test {
  protected:
    ecs::Entity spawnBullet(float x, float y) {
        const ecs::Entity e = registry.spawnEntity();
        client::prefabs::buildClientBullet(e, registry, ::prefabs::PrefabParams{{"x", x}, {"y", y}});
        return e;
    }

    ecs::Entity spawnEnemy(float x, float y) {
        const ecs::Entity e = registry.spawnEntity();
        client::prefabs::buildClientEnemy(
            e, registry,
            ::prefabs::PrefabParams{{"x", x}, {"y", y}, {"speed", 0.0F}, {"scaleX", 2.0F}, {"scaleY", 2.0F}});
        return e;
    }

    void run(int steps) {
        for (int i = 0; i < steps; ++i) {
            physics.update(STEP);
        }
    }

    ecs::Registry registry;
    ecs::EventBus bus;
    systems::PhysicsSystem physics{registry, bus};
    CollisionRouter router{bus};
    ShotHitRule rule{bus, registry};
};

TEST_F(ShotHitRuleTest, BulletDestroysTheEnemyItHitsAndItself) {
    const ecs::Entity bullet = spawnBullet(100.0F, 300.0F);
    const ecs::Entity enemy = spawnEnemy(300.0F, 300.0F);

    run(60);

    EXPECT_FALSE(registry.isAlive(enemy));
    EXPECT_FALSE(registry.isAlive(bullet));
    EXPECT_EQ(rule.destroyedEnemies(), 1U);
}

TEST_F(ShotHitRuleTest, BulletOnAnotherLaneMissesTheEnemy) {
    const ecs::Entity bullet = spawnBullet(100.0F, 100.0F);
    const ecs::Entity enemy = spawnEnemy(300.0F, 400.0F);

    run(60);

    EXPECT_TRUE(registry.isAlive(enemy));
    EXPECT_TRUE(registry.isAlive(bullet));
    EXPECT_EQ(rule.destroyedEnemies(), 0U);
}

TEST_F(ShotHitRuleTest, OneBulletDestroysOnlyOneOfTwoOverlappingEnemies) {
    spawnBullet(100.0F, 300.0F);
    const ecs::Entity first = spawnEnemy(300.0F, 300.0F);
    const ecs::Entity second = spawnEnemy(305.0F, 300.0F);

    run(60);

    EXPECT_EQ(rule.destroyedEnemies(), 1U);
    EXPECT_NE(registry.isAlive(first), registry.isAlive(second));
}

TEST_F(ShotHitRuleTest, EnemyShotsAreIgnored) {
    const ecs::Entity shot = registry.spawnEntity();
    const ecs::Entity target = registry.spawnEntity();

    bus.publish(ShotHitEvent{
        .shot = shot, .target = target, .targetCategory = ::prefabs::collision::PLAYER, .byPlayer = false});

    EXPECT_TRUE(registry.isAlive(shot));
    EXPECT_TRUE(registry.isAlive(target));
}

TEST_F(ShotHitRuleTest, ObstacleOnlyDestroysTheShot) {
    const ecs::Entity shot = registry.spawnEntity();
    const ecs::Entity wall = registry.spawnEntity();

    bus.publish(
        ShotHitEvent{.shot = shot, .target = wall, .targetCategory = ::prefabs::collision::OBSTACLE, .byPlayer = true});

    EXPECT_FALSE(registry.isAlive(shot));
    EXPECT_TRUE(registry.isAlive(wall));
    EXPECT_EQ(rule.destroyedEnemies(), 0U);
}

TEST(ClientPrefabTexture, ATextureAliasIsResolvedByTheAssetManager) {
    ecs::Registry registry;
    auto& manager = registry.emplaceResource<assets::AssetManager>();
    manager.setLoader<assets::Texture>(assets::AssetLoader<assets::Texture>{
        .load = [](const std::string& /*path*/, const assets::LoadOptions& /*options*/,
                   ecs::TextureId /*preferred*/) { return ecs::TextureId{7}; },
        .unload = [](ecs::TextureId /*id*/) {},
    });
    manager.declare<assets::Texture>("bydos", "sprites/spritesheet5.png");
    manager.loadGroup(assets::AssetManager::GLOBAL_GROUP);

    const ecs::Entity byAlias = registry.spawnEntity();
    client::prefabs::buildClientEnemy(byAlias, registry, ::prefabs::PrefabParams{{"texture", std::string("bydos")}});
    EXPECT_EQ(registry.getComponents<ecs::Renderable>().get(byAlias).textureId, 7U);

    const ecs::Entity unknown = registry.spawnEntity();
    client::prefabs::buildClientBullet(unknown, registry, ::prefabs::PrefabParams{{"texture", std::string("nope")}});
    EXPECT_EQ(registry.getComponents<ecs::Renderable>().get(unknown).textureId, ecs::NO_TEXTURE);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
