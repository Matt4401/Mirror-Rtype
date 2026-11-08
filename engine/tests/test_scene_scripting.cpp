#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include "components/ScriptComponent.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "scene/IScene.hpp"
#include "scene/SceneManager.hpp"
#include "scene/ScenePlugin.hpp"
#include "scene/SceneTypes.hpp"
#include "scene/scripting/SceneScriptingPlugin.hpp"
#include "scene/scripting/ScriptScene.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace {

/** @brief Marker of entities built by the test prefab. */
struct Dot {};

/** @brief Plain C++ scene, to mix with scripted ones. */
class GameScene final : public scene::IScene {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Game"; }
};

/** @brief Writes a Wren module next to the test binary's working directory and removes it afterwards. */
class ScriptFile {
  public:
    ScriptFile(std::string module, std::string_view source) : _module(std::move(module)) { write(source); }
    ~ScriptFile() {
        std::error_code ec;
        std::filesystem::remove(_module + ".wren", ec);
    }
    ScriptFile(const ScriptFile&) = delete;
    ScriptFile& operator=(const ScriptFile&) = delete;
    ScriptFile(ScriptFile&&) = delete;
    ScriptFile& operator=(ScriptFile&&) = delete;

    void write(std::string_view source) const {
        const std::string path = _module + ".wren";
        {
            std::ofstream out(path);
            out << source;
        }
        // Make every rewrite visible to the hot reload, even within the filesystem's time resolution.
        static int bump = 0;
        std::error_code ec;
        std::filesystem::last_write_time(
            path, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(++bump), ec);
    }

    [[nodiscard]] const std::string& module() const noexcept { return _module; }

  private:
    std::string _module;
};

/** @brief Records what the scripted scenes do, readable from C++ through `Log.text`. */
constexpr std::string_view LOG_MODULE = R"wren(
class Log {
    static entries { __entries = __entries || [] }
    static add(entry) { entries.add(entry) }
    static text { entries.join(",") }
    static clear() { __entries = [] }
    static frames { __frames = __frames || 0 }
    static tick() { __frames = frames + 1 }
}
)wren";

constexpr std::string_view TITLE_SCENE = R"wren(
import "TestSceneLog" for Log
import "core/Engine" for Engine

class Title {
    construct new() {}
    onEnter() {
        Log.add("enter")
        Engine.spawnPrefab("dot", 1, 2)
    }
    onExit() { Log.add("exit") }
    onPause() { Log.add("pause") }
    onResume() { Log.add("resume") }
    update(dt) { Log.tick() }
}
)wren";

constexpr std::string_view WAITING_SCENE = R"wren(
import "TestSceneLog" for Log
import "core/Engine" for Engine

class Waiting {
    construct new() {}
    onEnter() {
        Engine.wait(0.5)
        Log.add("woke")
    }
}
)wren";

/** @brief Engine with scenes, scripts and the bridge, plus a `dot` prefab and the shared log module. */
class SceneScriptingTest : public ::testing::Test {
  protected:
    void SetUp() override {
        engine.addPlugin<scene::ScenePlugin>();
        engine.addPlugin<scripting::ScriptPlugin>();
        engine.addPlugin<scene::SceneScriptingPlugin>();
        prefabs.registerPrefab("dot", [](ecs::Entity e, ecs::Registry& registry, const prefabs::PrefabParams&) {
            registry.addComponent(e, Dot{});
        });
        auto& bridge = engine.getPlugin<scene::SceneScriptingPlugin>();
        game = manager().registerScene<GameScene>();
        title = bridge.registerScriptScene(engine, "Title", titleFile.module(), "Title",
                                           scene::ScriptSceneOptions{.hasUpdate = true});
        waiting = bridge.registerScriptScene(engine, "Waiting", waitingFile.module(), "Waiting");
        engine.init();
        scripts().setPrefabManager(&prefabs);
    }

    scene::SceneManager& manager() { return engine.getPlugin<scene::ScenePlugin>().manager(); }
    systems::ScriptSystem& scripts() { return engine.getPlugin<scripting::ScriptPlugin>().scriptSystem(); }

    /** @brief Runs Wren code in a fresh module, returning whether it succeeded. */
    bool run(std::string_view code) {
        static int counter = 0;
        const std::string module = "test_snippet_" + std::to_string(++counter);
        return scripts().getVm().interpret(module, code).has_value();
    }

    /** @brief Reads a string expression evaluated in Wren. */
    std::string read(std::string_view expression) {
        static int counter = 0;
        const std::string module = "test_read_" + std::to_string(++counter);
        const std::string code = "import \"TestSceneLog\" for Log\nimport \"core/Scene\" for Scene\nvar result = \"%(" +
                                 std::string(expression) + ")\"\n";
        if (!scripts().getVm().interpret(module, code)) {
            return "<error>";
        }
        scripts().getVm().ensureSlots(1);
        scripts().getVm().getVariable(module, "result", 0);
        return scripts().getVm().getSlot<std::string>(0);
    }

    void frame() { (void)engine.advance(); }

    int countDots() {
        int count = 0;
        engine.registry().runSystem<Dot>([&](ecs::Entity /*e*/, Dot& /*dot*/) { ++count; });
        return count;
    }

    ScriptFile logFile{"TestSceneLog", LOG_MODULE};
    ScriptFile titleFile{"TestSceneTitle", TITLE_SCENE};
    ScriptFile waitingFile{"TestSceneWaiting", WAITING_SCENE};
    prefabs::PrefabManager prefabs;
    engine::Engine engine;
    scene::SceneId game = scene::INVALID_SCENE;
    scene::SceneId title = scene::INVALID_SCENE;
    scene::SceneId waiting = scene::INVALID_SCENE;
};

TEST_F(SceneScriptingTest, ScriptsChangeAndQueryTheStackByName) {
    ASSERT_TRUE(run("import \"core/Scene\" for Scene\nScene.switchTo(\"Game\")"));
    EXPECT_EQ(manager().top(), scene::INVALID_SCENE);
    frame();
    EXPECT_EQ(manager().top(), game);
    EXPECT_EQ(read("Scene.current"), "Game");
    EXPECT_EQ(read("Scene.isActive(\"Game\")"), "true");
    EXPECT_EQ(read("Scene.isRunning(\"Title\")"), "false");

    ASSERT_TRUE(run("import \"core/Scene\" for Scene\nScene.pop()"));
    frame();
    EXPECT_EQ(manager().top(), scene::INVALID_SCENE);
    EXPECT_EQ(read("Scene.current"), "null");
}

TEST_F(SceneScriptingTest, UnknownSceneNameAbortsTheFiber) {
    EXPECT_FALSE(run("import \"core/Scene\" for Scene\nScene.push(\"Nope\")"));
    EXPECT_FALSE(manager().hasPendingTransitions());
}

TEST_F(SceneScriptingTest, ScriptSceneHooksRunInOrder) {
    manager().switchTo(title);
    frame();
    frame();
    manager().push(game);
    frame();
    manager().pop();
    frame();
    manager().pop();
    frame();
    EXPECT_EQ(read("Log.text"), "enter,pause,resume,exit");
}

TEST_F(SceneScriptingTest, UpdateRunsOnlyWhileTheScriptSceneRuns) {
    manager().switchTo(title);
    frame();
    EXPECT_TRUE(static_cast<scene::ScriptScene&>(manager().scene(title)).hasInstance());
    frame();
    frame();
    EXPECT_EQ(read("Log.frames"), "3");

    manager().push(game);
    frame();
    frame();
    EXPECT_EQ(read("Log.frames"), "3");

    manager().pop();
    frame();
    EXPECT_EQ(read("Log.frames"), "4");
}

TEST_F(SceneScriptingTest, EntitiesSpawnedByAScriptSceneBelongToIt) {
    manager().switchTo(title);
    frame();
    frame();  // the script system flushes its spawns during update
    ASSERT_EQ(countDots(), 1);
    engine.registry().runSystem<Dot>([&](ecs::Entity e, Dot& /*dot*/) {
        EXPECT_EQ(engine.registry().getComponents<scene::SceneMember>().get(e).id, title);
    });

    manager().switchTo(game);
    frame();
    EXPECT_EQ(countDots(), 0);
}

TEST_F(SceneScriptingTest, EntitiesSpawnedByAScriptedEntityBelongToItsScene) {
    const ScriptFile spawner{"TestSceneSpawner", R"wren(
import "core/Engine" for Engine
class Spawner {
    construct new(entity) {}
    onSpawn() { Engine.spawnPrefab("dot", 0, 0) }
}
)wren"};
    manager().switchTo(game);
    frame();
    const ecs::Entity entity = manager().context(game).spawn();
    engine.registry().addComponent(entity,
                                   ecs::ScriptComponent{.moduleName = spawner.module(), .className = "Spawner"});
    frame();
    frame();
    ASSERT_EQ(countDots(), 1);
    engine.registry().runSystem<Dot>([&](ecs::Entity e, Dot& /*dot*/) {
        EXPECT_EQ(engine.registry().getComponents<scene::SceneMember>().get(e).id, game);
    });
}

TEST_F(SceneScriptingTest, SpawnsWithoutOwnerGoToTheTopExclusiveScene) {
    manager().switchTo(game);
    frame();
    ASSERT_TRUE(run("import \"core/Engine\" for Engine\nEngine.spawnPrefab(\"dot\", 0, 0)"));
    frame();
    ASSERT_EQ(countDots(), 1);
    engine.registry().runSystem<Dot>([&](ecs::Entity e, Dot& /*dot*/) {
        EXPECT_EQ(engine.registry().getComponents<scene::SceneMember>().get(e).id, game);
    });
}

TEST_F(SceneScriptingTest, FibersOfAnExitedSceneNeverResume) {
    manager().switchTo(waiting);
    frame();
    manager().switchTo(game);
    frame();
    scripts().update(1.0F);
    EXPECT_EQ(read("Log.text"), "");
}

TEST_F(SceneScriptingTest, FibersOfARunningSceneResume) {
    manager().switchTo(waiting);
    frame();
    scripts().update(1.0F);
    EXPECT_EQ(read("Log.text"), "woke");
}

TEST_F(SceneScriptingTest, HotReloadKeepsTheBindingsAndRebuildsTheScene) {
    manager().switchTo(title);
    frame();
    auto& titleScene = static_cast<scene::ScriptScene&>(manager().scene(title));
    ASSERT_TRUE(titleScene.hasInstance());

    titleFile.write(std::string(TITLE_SCENE) + "\n// edited\n");
    ASSERT_TRUE(scripts().reloadModifiedScripts());

    EXPECT_TRUE(titleScene.hasInstance());
    ASSERT_TRUE(run("import \"core/Scene\" for Scene\nScene.switchTo(\"Game\")"));
    frame();
    EXPECT_EQ(manager().top(), game);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
