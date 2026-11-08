#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPath.hpp"
#include "assets/AssetPlugin.hpp"
#include "core/Engine.hpp"
#include "ecs/Registry.hpp"

namespace {

/** @brief What a fake backend did. */
struct LoaderLog {
    /** @brief Paths loaded, in order. */
    std::vector<std::string> loads;
    /** @brief Handles released, in order. */
    std::vector<std::uint32_t> unloads;
    /** @brief Paths the fake backend refuses to load. */
    std::vector<std::string> missing;
    /** @brief Next handle. */
    std::uint32_t next = 1;
    /** @brief Options of each load. */
    std::vector<assets::LoadOptions> options;
};

/**
 * @brief Fake loader of `Kind` recording into `log` (shared, so it outlives the manager's copy of the lambdas).
 */
template <assets::AssetKind Kind>
assets::AssetLoader<Kind> fakeLoader(const std::shared_ptr<LoaderLog>& log) {
    return assets::AssetLoader<Kind>{
        .load = [log](const std::string& path, const assets::LoadOptions& options) -> typename Kind::Id {
            if (std::ranges::find(log->missing, path) != log->missing.end()) {
                return Kind::NONE;
            }
            log->loads.push_back(path);
            log->options.push_back(options);
            return static_cast<typename Kind::Id>(log->next++);
        },
        .unload = [log](typename Kind::Id id) { log->unloads.push_back(static_cast<std::uint32_t>(id)); },
    };
}

/** @brief Manager with a fake texture loader. */
class AssetManagerTest : public ::testing::Test {
  protected:
    void SetUp() override { manager.setLoader<assets::Texture>(fakeLoader<assets::Texture>(textures)); }

    std::shared_ptr<LoaderLog> textures = std::make_shared<LoaderLog>();
    assets::AssetManager manager;
};

/** @brief Temporary directory removed at the end of a test. */
class TempDirectory {
  public:
    TempDirectory() : _path(std::filesystem::temp_directory_path() / ("assets_test_" + std::to_string(counter()++))) {
        std::filesystem::create_directories(_path);
    }
    ~TempDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(_path, ec);
    }
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    TempDirectory(TempDirectory&&) = delete;
    TempDirectory& operator=(TempDirectory&&) = delete;

    void touch(const std::string& name) const { std::ofstream(_path / name) << "x"; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return _path; }

  private:
    static int& counter() {
        static int value = 0;
        return value;
    }
    std::filesystem::path _path;
};

}  // namespace

TEST(AssetPath, NormalisesSeparatorsAndDots) {
    EXPECT_EQ(assets::normalizeAssetPath("sprites/ship.png"), "sprites/ship.png");
    EXPECT_EQ(assets::normalizeAssetPath("sprites\\ship.png"), "sprites/ship.png");
    EXPECT_EQ(assets::normalizeAssetPath("a/../sprites/./ship.png"), "sprites/ship.png");
    EXPECT_EQ(assets::normalizeAssetPath("sprites//ship.png"), "sprites/ship.png");
    EXPECT_EQ(assets::normalizeAssetPath("textures/"), "textures");
}

TEST(AssetPath, RejectsEmptyTooLongAndNulPaths) {
    EXPECT_FALSE(assets::normalizeAssetPath(""));
    EXPECT_FALSE(assets::normalizeAssetPath("."));
    EXPECT_FALSE(assets::normalizeAssetPath(std::string(assets::MAX_PATH_LENGTH + 1, 'a')));
    EXPECT_FALSE(assets::normalizeAssetPath(std::string("a\0b", 3)));
    EXPECT_FALSE(assets::isValidAssetName(""));
    EXPECT_FALSE(assets::isValidAssetName(std::string(assets::MAX_NAME_LENGTH + 1, 'a')));
    EXPECT_TRUE(assets::isValidAssetName("player_ship"));
}

TEST_F(AssetManagerTest, LoadingTheSameFileAHundredTimesLoadsItOnce) {
    const auto first = manager.load<assets::Texture>("sprites/ship.png");
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(manager.load<assets::Texture>("sprites/ship.png"), first);
    }
    EXPECT_NE(first, assets::Texture::NONE);
    EXPECT_EQ(textures->loads.size(), 1U);
    EXPECT_EQ(manager.loadedCount<assets::Texture>(), 1U);
}

TEST_F(AssetManagerTest, AnAliasAndEveryWayOfWritingItsPathShareOneLoad) {
    ASSERT_TRUE(manager.declare<assets::Texture>("player_ship", "sprites/ship.png"));
    const auto byAlias = manager.load<assets::Texture>("player_ship");
    EXPECT_EQ(manager.load<assets::Texture>("sprites/ship.png"), byAlias);
    EXPECT_EQ(manager.load<assets::Texture>("other/../sprites/ship.png"), byAlias);
    EXPECT_EQ(manager.load<assets::Texture>("sprites\\ship.png"), byAlias);
    EXPECT_EQ(manager.get<assets::Texture>("player_ship"), byAlias);
    EXPECT_EQ(textures->loads, std::vector<std::string>{"sprites/ship.png"});
}

TEST_F(AssetManagerTest, DeclaringLoadsNothingUntilTheGroupIsLoaded) {
    ASSERT_TRUE(manager.declare<assets::Texture>("ship", "sprites/ship.png", "level1"));
    ASSERT_TRUE(manager.declare<assets::Texture>("enemy", "sprites/enemy.png", "level1"));
    EXPECT_TRUE(textures->loads.empty());
    EXPECT_EQ(manager.get<assets::Texture>("ship"), assets::Texture::NONE);

    EXPECT_TRUE(manager.loadGroup("level1"));
    EXPECT_EQ(textures->loads.size(), 2U);
    EXPECT_NE(manager.get<assets::Texture>("ship"), assets::Texture::NONE);
    EXPECT_TRUE(manager.isGroupLoaded("level1"));

    // Loading it again, or declaring the same assets again (script run twice), loads nothing more.
    EXPECT_TRUE(manager.loadGroup("level1"));
    EXPECT_TRUE(manager.declare<assets::Texture>("ship", "sprites/ship.png", "level1"));
    EXPECT_EQ(textures->loads.size(), 2U);
}

TEST_F(AssetManagerTest, AnAssetDeclaredInALoadedGroupIsLoadedAtOnce) {
    ASSERT_TRUE(manager.loadGroup("level1"));
    ASSERT_TRUE(manager.declare<assets::Texture>("boss", "sprites/boss.png", "level1"));
    EXPECT_NE(manager.get<assets::Texture>("boss"), assets::Texture::NONE);
}

TEST_F(AssetManagerTest, UnloadingAGroupReleasesOnlyWhatNoOtherGroupHolds) {
    manager.declare<assets::Texture>("ship", "sprites/ship.png", "global");
    manager.declare<assets::Texture>("ship", "sprites/ship.png", "level1");
    manager.declare<assets::Texture>("boss", "sprites/boss.png", "level1");
    manager.loadGroup("global");
    manager.loadGroup("level1");
    const auto ship = manager.get<assets::Texture>("ship");
    const auto boss = manager.get<assets::Texture>("boss");

    manager.unloadGroup("level1");
    EXPECT_EQ(textures->unloads, std::vector<std::uint32_t>{boss});
    EXPECT_EQ(manager.get<assets::Texture>("ship"), ship);
    EXPECT_EQ(manager.get<assets::Texture>("boss"), assets::Texture::NONE);
    EXPECT_FALSE(manager.isGroupLoaded("level1"));

    // The declarations are kept: entering the level again loads the boss again.
    manager.loadGroup("level1");
    EXPECT_NE(manager.get<assets::Texture>("boss"), assets::Texture::NONE);
    EXPECT_EQ(textures->loads.size(), 3U);
}

TEST_F(AssetManagerTest, GoingBackAndForthBetweenScenesKeepsTheLoadedCountFlat) {
    manager.declare<assets::Texture>("menu_bg", "sprites/menu.png", "menu");
    manager.declare<assets::Texture>("ship", "sprites/ship.png", "game");
    for (int i = 0; i < 10; ++i) {
        manager.loadGroup("menu");
        manager.unloadGroup("menu");
        manager.loadGroup("game");
        EXPECT_EQ(manager.loadedCount<assets::Texture>(), 1U);
        manager.unloadGroup("game");
    }
    EXPECT_EQ(manager.loadedCount<assets::Texture>(), 0U);
    EXPECT_EQ(textures->loads.size(), textures->unloads.size());
}

TEST_F(AssetManagerTest, UnloadAllReleasesEverythingOnce) {
    manager.load<assets::Texture>("a.png", "one");
    manager.load<assets::Texture>("b.png", "two");
    manager.load<assets::Texture>("a.png", "two");
    manager.unloadAll();
    EXPECT_EQ(textures->unloads.size(), 2U);
    EXPECT_EQ(manager.loadedCount<assets::Texture>(), 0U);
    manager.unloadAll();
    EXPECT_EQ(textures->unloads.size(), 2U);
}

TEST_F(AssetManagerTest, DetachingTheLoaderReleasesItsAssetsAndANewLoaderReloadsThem) {
    manager.load<assets::Texture>("a.png");
    manager.detachLoader<assets::Texture>();
    EXPECT_EQ(textures->unloads.size(), 1U);
    EXPECT_FALSE(manager.hasLoader<assets::Texture>());
    EXPECT_EQ(manager.get<assets::Texture>("a.png"), assets::Texture::NONE);

    auto next = std::make_shared<LoaderLog>();
    manager.setLoader<assets::Texture>(fakeLoader<assets::Texture>(next));
    EXPECT_EQ(next->loads, std::vector<std::string>{"a.png"});
}

TEST(AssetManager, RequestsMadeBeforeAnyLoaderAreServedWhenItIsInstalled) {
    assets::AssetManager manager;
    manager.declare<assets::Sound>("jump", "sounds/jump.wav");
    EXPECT_FALSE(manager.loadGroup("global"));
    EXPECT_EQ(manager.load<assets::Sound>("sounds/hit.wav"), assets::Sound::NONE);

    auto sounds = std::make_shared<LoaderLog>();
    manager.setLoader<assets::Sound>(fakeLoader<assets::Sound>(sounds));
    EXPECT_EQ(sounds->loads.size(), 2U);
    EXPECT_NE(manager.get<assets::Sound>("jump"), assets::Sound::NONE);
}

TEST_F(AssetManagerTest, AMissingFileIsReportedAndNotRetriedOnEveryRequest) {
    textures->missing.push_back("sprites/missing.png");
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(manager.load<assets::Texture>("sprites/missing.png"), assets::Texture::NONE);
    }
    EXPECT_EQ(manager.warnings().size(), 1U);
}

TEST_F(AssetManagerTest, InvalidInputIsRejectedWithAWarning) {
    EXPECT_FALSE(manager.declare<assets::Texture>("ship", ""));
    EXPECT_FALSE(manager.declare<assets::Texture>("ship", "a.png", ""));
    EXPECT_FALSE(manager.declare<assets::Texture>(std::string(assets::MAX_NAME_LENGTH + 1, 'x'), "a.png"));
    EXPECT_EQ(manager.load<assets::Texture>(""), assets::Texture::NONE);
    EXPECT_EQ(manager.load<assets::Texture>("a.png", ""), assets::Texture::NONE);
    EXPECT_FALSE(manager.loadGroup(""));
    EXPECT_TRUE(textures->loads.empty());
    EXPECT_EQ(manager.warnings().size(), 6U);
    manager.clearWarnings();
    EXPECT_TRUE(manager.warnings().empty());
}

TEST_F(AssetManagerTest, AnAliasDeclaredAgainForAnotherFileNamesTheNewFile) {
    manager.declare<assets::Texture>("ship", "sprites/old.png");
    manager.declare<assets::Texture>("ship", "sprites/new.png");
    manager.loadGroup("global");
    EXPECT_EQ(manager.warnings().size(), 1U);
    const auto ship = manager.get<assets::Texture>("ship");
    EXPECT_EQ(manager.get<assets::Texture>("sprites/new.png"), ship);
}

TEST_F(AssetManagerTest, TheOptionsOfTheFirstDeclarationAreKept) {
    manager.declare<assets::Texture>("a", "a.png", "global", {.filter = assets::TextureFilter::Smooth});
    manager.declare<assets::Texture>("b", "a.png", "global", {.filter = assets::TextureFilter::Nearest});
    manager.loadGroup("global");
    ASSERT_EQ(textures->options.size(), 1U);
    EXPECT_EQ(textures->options.front().filter, assets::TextureFilter::Smooth);
}

TEST_F(AssetManagerTest, KindsHaveSeparateAliases) {
    auto sounds = std::make_shared<LoaderLog>();
    manager.setLoader<assets::Sound>(fakeLoader<assets::Sound>(sounds));
    manager.declare<assets::Texture>("jump", "sprites/jump.png");
    manager.declare<assets::Sound>("jump", "sounds/jump.wav");
    manager.loadGroup("global");
    EXPECT_EQ(textures->loads, std::vector<std::string>{"sprites/jump.png"});
    EXPECT_EQ(sounds->loads, std::vector<std::string>{"sounds/jump.wav"});
}

TEST_F(AssetManagerTest, PreloadDirectoryAliasesFilesByStemInNameOrder) {
    const TempDirectory directory;
    directory.touch("b_ship.png");
    directory.touch("a_star.PNG");
    directory.touch("notes.txt");
    directory.touch("c_sound.wav");

    EXPECT_EQ(manager.preloadDirectory<assets::Texture>(directory.path(), "level1"), 2U);
    ASSERT_EQ(textures->loads.size(), 2U);
    EXPECT_TRUE(textures->loads[0].ends_with("a_star.PNG"));
    EXPECT_TRUE(textures->loads[1].ends_with("b_ship.png"));
    EXPECT_NE(manager.get<assets::Texture>("a_star"), assets::Texture::NONE);
    EXPECT_NE(manager.get<assets::Texture>("b_ship"), assets::Texture::NONE);
    EXPECT_EQ(manager.get<assets::Texture>("notes"), assets::Texture::NONE);

    manager.unloadGroup("level1");
    EXPECT_EQ(manager.loadedCount<assets::Texture>(), 0U);
}

TEST_F(AssetManagerTest, PreloadingAMissingDirectoryWarns) {
    EXPECT_EQ(manager.preloadDirectory<assets::Texture>("does/not/exist"), 0U);
    EXPECT_EQ(manager.warnings().size(), 1U);
}

TEST(AssetPlugin, AddsTheManagerAndReleasesEverythingAtShutdown) {
    engine::Engine engine;
    engine.addPlugin<assets::AssetPlugin>();
    engine.init();
    ASSERT_TRUE(engine.registry().hasResource<assets::AssetManager>());

    auto log = std::make_shared<LoaderLog>();
    auto& manager = engine.registry().getResource<assets::AssetManager>();
    manager.setLoader<assets::Music>(fakeLoader<assets::Music>(log));
    manager.load<assets::Music>("music/theme.ogg");

    engine.shutdown();
    EXPECT_EQ(log->unloads.size(), 1U);
    EXPECT_FALSE(engine.registry().hasResource<assets::AssetManager>());
}

TEST(AssetManager, RelativePathsAreLoadedFromTheRootAndKeptRelative) {
    const TempDirectory root;
    std::filesystem::create_directories(root.path() / "textures");
    std::ofstream(root.path() / "textures" / "car.png") << "x";

    assets::AssetManager manager(root.path());
    auto textures = std::make_shared<LoaderLog>();
    manager.setLoader<assets::Texture>(fakeLoader<assets::Texture>(textures));

    const auto ship = manager.load<assets::Texture>("sprites/ship.png");
    const std::filesystem::path absolute = root.path() / "absolute.png";
    manager.load<assets::Texture>(absolute.generic_string());
    EXPECT_EQ(manager.preloadDirectory<assets::Texture>("textures", "level"), 1U);

    ASSERT_EQ(textures->loads.size(), 3U);
    EXPECT_EQ(textures->loads[0], (root.path() / "sprites/ship.png").generic_string());
    EXPECT_EQ(textures->loads[1], absolute.generic_string());
    EXPECT_EQ(textures->loads[2], (root.path() / "textures/car.png").generic_string());
    // The keys stay as declared: the same names work wherever the game is installed.
    EXPECT_EQ(manager.get<assets::Texture>("sprites/ship.png"), ship);
    EXPECT_NE(manager.get<assets::Texture>("textures/car.png"), assets::Texture::NONE);
    EXPECT_NE(manager.get<assets::Texture>("car"), assets::Texture::NONE);
    EXPECT_EQ(manager.root(), root.path());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
