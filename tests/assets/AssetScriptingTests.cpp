#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPlugin.hpp"
#include "assets/scripting/AssetScriptingPlugin.hpp"
#include "core/Engine.hpp"
#include "ecs/Registry.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace {

/** @brief Module name of the declaration script written by the tests. */
constexpr std::string_view DECLARATIONS = "TestAssetDeclarations";

constexpr std::string_view DECLARATIONS_SOURCE = R"wren(
import "core/Assets" for Assets

Assets.texture("ship", "sprites/ship.png")
Assets.smoothTexture("stadium", "art/stadium.png", "match")
Assets.sound("jump", "sounds/jump.wav", "match")
Assets.font("ui", "fonts/ui.ttf")
Assets.music("theme", "music/theme.ogg")
Assets.loadGroup("global")
)wren";

/** @brief Writes a Wren module in the working directory and removes it afterwards. */
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

  private:
    std::string _module;
};

/** @brief What a fake backend loaded. */
struct LoaderLog {
    /** @brief Paths loaded, in order. */
    std::vector<std::string> loads;
    /** @brief Options of each load. */
    std::vector<assets::LoadOptions> options;
    /** @brief Handles released. */
    std::vector<std::uint32_t> unloads;
};

template <assets::AssetKind Kind>
assets::AssetLoader<Kind> fakeLoader(const std::shared_ptr<LoaderLog>& log) {
    return assets::AssetLoader<Kind>{
        .load = [log](const std::string& path, const assets::LoadOptions& options, typename Kind::Id /*preferred*/) ->
        typename Kind::Id {
            log->loads.push_back(path);
            log->options.push_back(options);
            return static_cast<typename Kind::Id>(log->loads.size());
        },
        .unload = [log](typename Kind::Id id) { log->unloads.push_back(static_cast<std::uint32_t>(id)); },
    };
}

/** @brief Engine running the asset bridge with the declaration script, and fake loaders of every kind. */
class AssetScriptingTest : public ::testing::Test {
  protected:
    void SetUp() override {
        engine.addPlugin<scripting::ScriptPlugin>()
            .addPlugin<assets::AssetPlugin>()
            .addPlugin<assets::AssetScriptingPlugin>(std::string(DECLARATIONS));
        engine.init();
        // Installed after the script ran: what it asked for is loaded now.
        manager().setLoader<assets::Texture>(fakeLoader<assets::Texture>(textures));
        manager().setLoader<assets::Sound>(fakeLoader<assets::Sound>(sounds));
        manager().setLoader<assets::Font>(fakeLoader<assets::Font>(fonts));
        manager().setLoader<assets::Music>(fakeLoader<assets::Music>(musics));
    }

    void TearDown() override { engine.shutdown(); }

    assets::AssetManager& manager() { return engine.registry().getResource<assets::AssetManager>(); }
    systems::ScriptSystem& scripts() { return engine.getPlugin<scripting::ScriptPlugin>().scriptSystem(); }

    /** @brief Runs Wren code in a fresh module; false if it failed (aborted fiber included). */
    bool run(std::string_view source) {
        static int counter = 0;
        const std::string module = "asset_test_" + std::to_string(++counter);
        return scripts()
            .getVm()
            .interpret(module, "import \"core/Assets\" for Assets\n" + std::string(source))
            .has_value();
    }

    ScriptFile declarations{std::string(DECLARATIONS), DECLARATIONS_SOURCE};
    engine::Engine engine;
    std::shared_ptr<LoaderLog> textures = std::make_shared<LoaderLog>();
    std::shared_ptr<LoaderLog> sounds = std::make_shared<LoaderLog>();
    std::shared_ptr<LoaderLog> fonts = std::make_shared<LoaderLog>();
    std::shared_ptr<LoaderLog> musics = std::make_shared<LoaderLog>();
};

}  // namespace

TEST_F(AssetScriptingTest, TheDeclarationScriptRunsAtInitAndLoadsItsGlobalGroup) {
    EXPECT_TRUE(engine.getPlugin<assets::AssetScriptingPlugin>().declarationsLoaded());
    EXPECT_EQ(textures->loads, std::vector<std::string>{"sprites/ship.png"});
    EXPECT_EQ(fonts->loads, std::vector<std::string>{"fonts/ui.ttf"});
    EXPECT_EQ(musics->loads, std::vector<std::string>{"music/theme.ogg"});
    EXPECT_TRUE(sounds->loads.empty());
    EXPECT_NE(manager().get<assets::Texture>("ship"), assets::Texture::NONE);
    EXPECT_EQ(manager().get<assets::Texture>("stadium"), assets::Texture::NONE);
}

TEST_F(AssetScriptingTest, AScriptLoadsAndUnloadsAGroup) {
    ASSERT_TRUE(run(R"(
        if (!Assets.loadGroup("match")) Fiber.abort("the match group should load")
        if (!Assets.isGroupLoaded("match")) Fiber.abort("the match group should be loaded")
        if (Assets.soundId("jump") == 0) Fiber.abort("jump should be loaded")
        if (!Assets.isLoaded("stadium")) Fiber.abort("stadium should be loaded")
    )"));
    EXPECT_EQ(sounds->loads, std::vector<std::string>{"sounds/jump.wav"});
    ASSERT_EQ(textures->options.size(), 2U);
    EXPECT_EQ(textures->options.back().filter, assets::TextureFilter::Smooth);

    ASSERT_TRUE(run(R"(
        Assets.unloadGroup("match")
        if (Assets.isGroupLoaded("match")) Fiber.abort("the match group should be unloaded")
        if (Assets.soundId("jump") != 0) Fiber.abort("jump should be unloaded")
        if (Assets.textureId("ship") == 0) Fiber.abort("ship is global and stays")
    )"));
    EXPECT_EQ(sounds->unloads.size(), 1U);
}

TEST_F(AssetScriptingTest, TheIdsSeenByScriptsAreTheOnesOfTheManager) {
    const auto ship = manager().get<assets::Texture>("ship");
    ASSERT_TRUE(run("if (Assets.textureId(\"ship\") != " + std::to_string(ship) + ") Fiber.abort(\"wrong id\")"));
    ASSERT_TRUE(run(R"(if (Assets.textureId("unknown") != 0) Fiber.abort("unknown should be 0"))"));
}

TEST_F(AssetScriptingTest, ArgumentsOfTheWrongTypeAbortTheFiberWithoutTouchingTheManager) {
    EXPECT_FALSE(run(R"(Assets.texture(1, "a.png"))"));
    EXPECT_FALSE(run(R"(Assets.sound("a", null))"));
    EXPECT_FALSE(run(R"(Assets.loadGroup(42))"));
    EXPECT_FALSE(run(R"(Assets.declare_(9, "a", "a.png", "global", false))"));
    EXPECT_FALSE(run(R"(Assets.declare_("shader", "a", "a.png", "global", false))"));
    EXPECT_FALSE(run(R"(Assets.declare_("texture", "a", "a.png", "global", 1))"));
    EXPECT_FALSE(run(R"(Assets.loadGroupStep("global", -1))"));
    EXPECT_FALSE(run(R"(Assets.preloadTextures([], "global"))"));
    EXPECT_EQ(manager().get<assets::Texture>("a.png"), assets::Texture::NONE);
    EXPECT_EQ(textures->loads.size(), 1U);
}

TEST_F(AssetScriptingTest, InvalidPathsAndNamesAreRejectedWithAWarning) {
    manager().clearWarnings();
    ASSERT_TRUE(run(R"(
        if (Assets.texture("a", "")) Fiber.abort("an empty path is invalid")
        if (Assets.texture("a", "a.png", "")) Fiber.abort("an empty group is invalid")
        if (Assets.loadGroup("")) Fiber.abort("an empty group cannot be loaded")
        if (Assets.preloadTextures("does/not/exist", "global") != 0) Fiber.abort("nothing to preload")
        if (Assets.preloadTextures("", "global") != 0) Fiber.abort("nothing to preload")
    )"));
    EXPECT_EQ(manager().warnings().size(), 5U);
}

TEST_F(AssetScriptingTest, ScriptsCannotNameFilesOutsideTheWorkingDirectory) {
    manager().clearWarnings();
    ASSERT_TRUE(run(R"(
        if (Assets.texture("a", "/etc/passwd")) Fiber.abort("an absolute path is refused")
        if (Assets.texture("b", "sprites/../../secret.png")) Fiber.abort("climbing out is refused")
        if (Assets.preloadTextures("..", "global") != 0) Fiber.abort("climbing out is refused")
    )"));
    EXPECT_EQ(manager().warnings().size(), 3U);
    EXPECT_EQ(manager().status<assets::Texture>("a"), assets::AssetStatus::Unknown);
}

TEST_F(AssetScriptingTest, ScriptsCannotUnloadTheGlobalGroup) {
    manager().clearWarnings();
    ASSERT_TRUE(run(R"(
        Assets.unloadGroup("global")
        Assets.forgetGroup("global")
        if (!Assets.isGroupLoaded("global")) Fiber.abort("global stays loaded")
        if (Assets.textureId("ship") == 0) Fiber.abort("ship stays loaded")
    )"));
    EXPECT_EQ(manager().warnings().size(), 2U);
}

TEST_F(AssetScriptingTest, AScriptLoadsAGroupStepByStep) {
    ASSERT_TRUE(run(R"(
        if (Assets.groupProgress("match") != 0) Fiber.abort("nothing loaded yet")
        if (Assets.loadGroupStep("match", 1)) Fiber.abort("two assets: one step is not enough")
        if (Assets.groupProgress("match") != 0.5) Fiber.abort("half of the match group")
        if (!Assets.loadGroupStep("match", 1)) Fiber.abort("done after the second step")
        if (Assets.status("sound", "jump") != "loaded") Fiber.abort("jump should be loaded")
        if (Assets.status("sound", "nope") != "unknown") Fiber.abort("nope is unknown")
    )"));
}

TEST_F(AssetScriptingTest, ScriptsReachEveryKindByName) {
    ASSERT_TRUE(run(R"(
        if (!Assets.hasKind("music")) Fiber.abort("music is a kind")
        if (Assets.hasKind("shader")) Fiber.abort("shader is not")
        if (!Assets.declare("music", "boss_theme", "music/boss.ogg", "global")) Fiber.abort("declared")
        if (Assets.id("music", "boss_theme") == 0) Fiber.abort("global is loaded: loaded at once")
        if (!Assets.undeclare("music", "boss_theme")) Fiber.abort("undeclared")
        if (Assets.musicId("boss_theme") != 0) Fiber.abort("released")
    )"));
    EXPECT_EQ(musics->unloads.size(), 1U);
}

TEST_F(AssetScriptingTest, HotReloadingTheDeclarationsLoadsNothingTwice) {
    declarations.write(DECLARATIONS_SOURCE);
    ASSERT_TRUE(scripts().reloadModifiedScripts());
    EXPECT_EQ(textures->loads.size(), 1U);
    EXPECT_EQ(fonts->loads.size(), 1U);
    // The bindings survive the new VM.
    ASSERT_TRUE(run(R"(if (Assets.textureId("ship") == 0) Fiber.abort("ship should still be loaded"))"));
}

TEST_F(AssetScriptingTest, HotReloadingWithoutALineReleasesItsAsset) {
    ASSERT_NE(manager().get<assets::Music>("theme"), assets::Music::NONE);
    std::string source(DECLARATIONS_SOURCE);
    const std::string line = "Assets.music(\"theme\", \"music/theme.ogg\")\n";
    source.erase(source.find(line), line.size());
    declarations.write(source);
    ASSERT_TRUE(scripts().reloadModifiedScripts());
    EXPECT_EQ(manager().status<assets::Music>("theme"), assets::AssetStatus::Unknown);
    EXPECT_EQ(musics->unloads.size(), 1U);
    EXPECT_EQ(manager().status<assets::Texture>("ship"), assets::AssetStatus::Loaded);
    EXPECT_EQ(textures->loads.size(), 1U);
}

TEST(AssetScriptingPlugin, AMissingDeclarationModuleIsReported) {
    engine::Engine engine;
    engine.addPlugin<scripting::ScriptPlugin>()
        .addPlugin<assets::AssetPlugin>()
        .addPlugin<assets::AssetScriptingPlugin>("does/not/Exist");
    engine.init();
    EXPECT_FALSE(engine.getPlugin<assets::AssetScriptingPlugin>().declarationsLoaded());
    engine.shutdown();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
