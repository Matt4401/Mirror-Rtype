#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPlugin.hpp"
#include "assets/scene/SceneAssetsPlugin.hpp"
#include "core/Engine.hpp"
#include "ecs/Registry.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "scene/IScene.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneManager.hpp"
#include "scene/ScenePlugin.hpp"
#include "scene/SceneTypes.hpp"

namespace {

/** @brief Scene running a hook in `onEnter`. */
class HookScene final : public scene::IScene {
  public:
    HookScene(std::string name, std::function<void()> hook) : _name(std::move(name)), _hook(std::move(hook)) {}

    [[nodiscard]] std::string_view name() const noexcept override { return _name; }

    void onEnter(scene::SceneContext& /*context*/) override {
        if (_hook) {
            _hook();
        }
    }

  private:
    std::string _name;
    std::function<void()> _hook;
};

/** @brief Engine with scenes, the asset manager, the scene bridge and a fake texture loader. */
class SceneAssetsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        engine.addPlugin<scene::ScenePlugin>()
            .addPlugin<hierarchy::HierarchyPlugin>()
            .addPlugin<assets::AssetPlugin>()
            .addPlugin<assets::SceneAssetsPlugin>();
        engine.init();
        manager().setLoader<assets::Texture>(assets::AssetLoader<assets::Texture>{
            .load =
                [this](const std::string& path, const assets::LoadOptions& /*options*/, ecs::TextureId /*preferred*/) {
                    loads.push_back(path);
                    return static_cast<ecs::TextureId>(loads.size());
                },
            .unload = [this](ecs::TextureId id) { unloads.push_back(id); },
            .fallback = {},
            .reserve = {},
        });
        manager().declare<assets::Texture>("boss", "sprites/boss.png", "Level1");
        manager().declare<assets::Texture>("ship", "sprites/ship.png", "Level1");
        manager().declare<assets::Texture>("ship", "sprites/ship.png", "global");
        manager().loadGroup("global");
    }

    void TearDown() override { engine.shutdown(); }

    assets::AssetManager& manager() { return engine.registry().getResource<assets::AssetManager>(); }
    scene::SceneManager& scenes() { return engine.getPlugin<scene::ScenePlugin>().manager(); }

    std::vector<std::string> loads;
    std::vector<ecs::TextureId> unloads;
    engine::Engine engine;
};

}  // namespace

TEST_F(SceneAssetsTest, ASceneFindsItsGroupLoadedInOnEnterAndReleasesItOnExit) {
    ecs::TextureId seenInOnEnter = ecs::NO_TEXTURE;
    const scene::SceneId level =
        scenes().registerScene<HookScene>("Level1", [&] { seenInOnEnter = manager().get<assets::Texture>("boss"); });
    scenes().push(level);
    scenes().applyTransitions();
    EXPECT_NE(seenInOnEnter, ecs::NO_TEXTURE);
    EXPECT_TRUE(manager().isGroupLoaded("Level1"));

    scenes().pop();
    scenes().applyTransitions();
    EXPECT_FALSE(manager().isGroupLoaded("Level1"));
    EXPECT_EQ(manager().get<assets::Texture>("boss"), ecs::NO_TEXTURE);
    // Shared with the global group: kept.
    EXPECT_NE(manager().get<assets::Texture>("ship"), ecs::NO_TEXTURE);
    EXPECT_EQ(unloads, std::vector<ecs::TextureId>{seenInOnEnter});
}

TEST_F(SceneAssetsTest, ASceneThatFailsToEnterReleasesItsGroup) {
    const scene::SceneId level =
        scenes().registerScene<HookScene>("Level1", [] { throw std::runtime_error("broken level"); });
    scenes().push(level);
    EXPECT_THROW(scenes().applyTransitions(), std::runtime_error);
    EXPECT_FALSE(manager().isGroupLoaded("Level1"));
    EXPECT_EQ(manager().get<assets::Texture>("boss"), ecs::NO_TEXTURE);
}

TEST_F(SceneAssetsTest, ASceneNamedGlobalNeverReleasesTheGlobalGroup) {
    const scene::SceneId global = scenes().registerScene<HookScene>("global", nullptr);
    scenes().push(global);
    scenes().applyTransitions();
    scenes().pop();
    scenes().applyTransitions();
    EXPECT_TRUE(manager().isGroupLoaded("global"));
    EXPECT_NE(manager().get<assets::Texture>("ship"), ecs::NO_TEXTURE);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
