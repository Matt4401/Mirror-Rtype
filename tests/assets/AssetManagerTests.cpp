#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPath.hpp"
#include "assets/AssetPlugin.hpp"
#include "core/Engine.hpp"
#include "ecs/Registry.hpp"
#include "utils/StableSlots.hpp"

namespace {

/** @brief What a fake backend did. */
struct LoaderLog {
    /** @brief Paths loaded, in order. */
    std::vector<std::string> loads;
    /** @brief Handles released, in order. */
    std::vector<std::uint32_t> unloads;
    /** @brief Paths the fake backend refuses to load. */
    std::vector<std::string> missing;
    /** @brief Next fresh handle. */
    std::uint32_t next = 1;
    /** @brief Options of each load. */
    std::vector<assets::LoadOptions> options;
    /** @brief `preferred` of each load. */
    std::vector<std::uint32_t> preferred;
    /** @brief Highest handle reserved, 0 if `reserve` was never called. */
    std::uint32_t reserved = 0;
    /** @brief Handle served for failed assets, 0 for none. */
    std::uint32_t fallback = 0;
};

/**
 * @brief Fake loader of `Kind` recording into `log` (shared, so it outlives the manager's copy of the lambdas). It
 * gives `preferred` back like the engine's stores do.
 */
template <assets::AssetKind Kind>
assets::AssetLoader<Kind> fakeLoader(const std::shared_ptr<LoaderLog>& log) {
    return assets::AssetLoader<Kind>{
        .load = [log](const std::string& path, const assets::LoadOptions& options, typename Kind::Id preferred) ->
        typename Kind::Id {
            if (std::ranges::find(log->missing, path) != log->missing.end()) {
                return Kind::NONE;
            }
            log->loads.push_back(path);
            log->options.push_back(options);
            log->preferred.push_back(static_cast<std::uint32_t>(preferred));
            if (preferred != Kind::NONE) {
                return preferred;
            }
            log->next = std::max(log->next, log->reserved + 1);
            return static_cast<typename Kind::Id>(log->next++);
        },
        .unload = [log](typename Kind::Id id) { log->unloads.push_back(static_cast<std::uint32_t>(id)); },
        .fallback = [log]() { return static_cast<typename Kind::Id>(log->fallback); },
        .reserve = [log](typename Kind::Id last) { log->reserved = static_cast<std::uint32_t>(last); },
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

/** @brief A kind no engine module knows: levels, defined by a game. */
struct Level {
    using Id = std::uint32_t;
    static constexpr Id NONE = 0;
    static constexpr std::string_view NAME = "level";
    static constexpr std::array<std::string_view, 1> EXTENSIONS = {".json"};
};

/** @brief A kind clashing with the engine's texture kind. */
struct OtherTexture {
    using Id = std::uint32_t;
    static constexpr Id NONE = 0;
    static constexpr std::string_view NAME = "texture";
    static constexpr std::array<std::string_view, 1> EXTENSIONS = {".png"};
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

TEST(AssetPath, AliasesCannotLookLikePaths) {
    EXPECT_TRUE(assets::isValidAssetAlias("player_ship"));
    EXPECT_FALSE(assets::isValidAssetAlias("ship.png"));
    EXPECT_FALSE(assets::isValidAssetAlias("sprites/ship"));
    EXPECT_FALSE(assets::isValidAssetAlias("sprites\\ship"));
    EXPECT_TRUE(assets::looksLikeAssetPath("ship.png"));
    EXPECT_FALSE(assets::looksLikeAssetPath("ship"));
}

TEST(AssetPath, TellsWhetherAPathStaysInTheWorkingDirectory) {
    EXPECT_TRUE(assets::isConfinedAssetPath("sprites/ship.png"));
    EXPECT_TRUE(assets::isConfinedAssetPath("..data/x.png"));
    EXPECT_FALSE(assets::isConfinedAssetPath("/etc/passwd"));
    EXPECT_FALSE(assets::isConfinedAssetPath("../secret.png"));
    EXPECT_FALSE(assets::isConfinedAssetPath(".."));
    EXPECT_FALSE(assets::isConfinedAssetPath("C:/Windows/x.png"));
    EXPECT_EQ(assets::normalizeAssetPath("a/../../b.png"), "../b.png");
}

TEST(AssetPath, QuotesAndCutsLongNamesForLogs) {
    EXPECT_EQ(assets::quoteForLog("ship"), "'ship'");
    EXPECT_EQ(assets::quoteForLog(std::string(500, 'x'), 4), "'xxxx...'");
}

TEST(StableSlots, GivesAFreedSlotBackOnlyToTheHandleThatAsksForIt) {
    struct Slot {
        int value = 0;
        bool loaded = false;
    };
    std::vector<Slot> slots;
    EXPECT_EQ(ecs::placeInSlot(slots, Slot{.value = 1, .loaded = true}, 0U), 1U);
    EXPECT_EQ(ecs::placeInSlot(slots, Slot{.value = 2, .loaded = true}, 0U), 2U);
    slots[0] = Slot{};
    // A fresh load never takes the freed slot...
    EXPECT_EQ(ecs::placeInSlot(slots, Slot{.value = 3, .loaded = true}, 0U), 3U);
    // ...the asset that had it gets it back...
    EXPECT_EQ(ecs::placeInSlot(slots, Slot{.value = 1, .loaded = true}, 1U), 1U);
    // ...and a taken slot is never overwritten.
    EXPECT_EQ(ecs::placeInSlot(slots, Slot{.value = 4, .loaded = true}, 2U), 4U);
    EXPECT_EQ(slots[1].value, 2);

    ecs::reserveSlots(slots, 10U);
    EXPECT_EQ(ecs::placeInSlot(slots, Slot{.value = 5, .loaded = true}, 0U), 11U);
    // A wild handle cannot make the store allocate millions of slots.
    EXPECT_EQ(ecs::placeInSlot(slots, Slot{.value = 6, .loaded = true}, 4'000'000'000U), 12U);
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
    EXPECT_EQ(manager.status<assets::Texture>("ship"), assets::AssetStatus::Declared);

    EXPECT_TRUE(manager.loadGroup("level1"));
    EXPECT_EQ(textures->loads.size(), 2U);
    EXPECT_NE(manager.get<assets::Texture>("ship"), assets::Texture::NONE);
    EXPECT_EQ(manager.status<assets::Texture>("ship"), assets::AssetStatus::Loaded);
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

TEST_F(AssetManagerTest, AnAssetLoadedAgainGetsItsHandleBack) {
    manager.declare<assets::Texture>("boss", "sprites/boss.png", "level1");
    manager.loadGroup("level1");
    const auto boss = manager.get<assets::Texture>("boss");
    manager.unloadGroup("level1");
    manager.loadGroup("level1");
    // A component or C++ code that kept the handle sees the boss again.
    EXPECT_EQ(manager.get<assets::Texture>("boss"), boss);
    EXPECT_EQ(textures->preferred.back(), boss);
}

TEST_F(AssetManagerTest, ANewLoaderReservesTheOldHandlesAndGivesThemBack) {
    manager.declare<assets::Texture>("a", "a.png");
    manager.declare<assets::Texture>("b", "b.png", "later");
    manager.loadGroup("global");
    manager.loadGroup("later");
    const auto a = manager.get<assets::Texture>("a");
    const auto b = manager.get<assets::Texture>("b");
    manager.unloadGroup("later");

    // The backend restarts (new store): every former handle is reserved, then the held ones are asked for again.
    auto next = std::make_shared<LoaderLog>();
    manager.setLoader<assets::Texture>(fakeLoader<assets::Texture>(next));
    EXPECT_EQ(next->reserved, std::max(a, b));
    EXPECT_EQ(manager.get<assets::Texture>("a"), a);
    // A fresh file cannot take b's former handle, so b gets it back when its group loads again.
    EXPECT_GT(manager.load<assets::Texture>("c.png"), std::max(a, b));
    manager.loadGroup("later");
    EXPECT_EQ(manager.get<assets::Texture>("b"), b);
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

TEST_F(AssetManagerTest, WhatLoadPutsInAGroupComesBackWithTheGroup) {
    manager.loadGroup("level1");
    manager.load<assets::Texture>("sprites/boss.png", "level1");
    manager.unloadGroup("level1");
    EXPECT_EQ(manager.get<assets::Texture>("sprites/boss.png"), assets::Texture::NONE);
    EXPECT_TRUE(manager.loadGroup("level1"));
    EXPECT_NE(manager.get<assets::Texture>("sprites/boss.png"), assets::Texture::NONE);
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
    EXPECT_EQ(manager.groupProgress("global").waiting, 1U);
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
    EXPECT_EQ(manager.status<assets::Texture>("sprites/missing.png"), assets::AssetStatus::Failed);
}

TEST_F(AssetManagerTest, AFailedAssetIsServedAsTheFallbackButStillReportedAsMissing) {
    textures->missing.push_back("sprites/missing.png");
    textures->fallback = 99;
    manager.declare<assets::Texture>("ship", "sprites/missing.png");
    manager.declare<assets::Texture>("star", "sprites/star.png");

    EXPECT_FALSE(manager.loadGroup("global"));
    EXPECT_EQ(manager.get<assets::Texture>("ship"), 99U);
    EXPECT_EQ(manager.status<assets::Texture>("ship"), assets::AssetStatus::Failed);
    EXPECT_EQ(manager.loadedCount<assets::Texture>(), 1U);
    const assets::GroupProgress progress = manager.groupProgress("global");
    EXPECT_EQ(progress.failed, 1U);
    EXPECT_EQ(progress.loaded, 1U);
    ASSERT_EQ(manager.warnings().size(), 1U);
    EXPECT_NE(manager.warnings().front().find("sprites/missing.png"), std::string::npos);

    // The fallback belongs to the backend: never released by the manager.
    manager.unloadAll();
    EXPECT_EQ(std::ranges::count(textures->unloads, 99U), 0);
}

TEST_F(AssetManagerTest, InvalidInputIsRejectedWithAWarning) {
    EXPECT_FALSE(manager.declare<assets::Texture>("ship", ""));
    EXPECT_FALSE(manager.declare<assets::Texture>("ship", "a.png", ""));
    EXPECT_FALSE(manager.declare<assets::Texture>(std::string(assets::MAX_NAME_LENGTH + 1, 'x'), "a.png"));
    EXPECT_FALSE(manager.declare<assets::Texture>("ship.png", "a.png"));
    EXPECT_EQ(manager.load<assets::Texture>(""), assets::Texture::NONE);
    EXPECT_EQ(manager.load<assets::Texture>("a.png", ""), assets::Texture::NONE);
    EXPECT_FALSE(manager.loadGroup(""));
    EXPECT_TRUE(textures->loads.empty());
    EXPECT_EQ(manager.warnings().size(), 7U);
    manager.clearWarnings();
    EXPECT_TRUE(manager.warnings().empty());
}

TEST_F(AssetManagerTest, WarningsCutHugeNames) {
    manager.declare<assets::Texture>(std::string(100'000, 'x'), "");
    ASSERT_EQ(manager.warnings().size(), 1U);
    EXPECT_LT(manager.warnings().front().size(), 2U * assets::MAX_PATH_LENGTH);
}

TEST_F(AssetManagerTest, AnUnknownAliasIsNeverLookedForOnTheDisk) {
    EXPECT_EQ(manager.load<assets::Texture>("player_shp"), assets::Texture::NONE);
    EXPECT_EQ(manager.load<assets::Texture>("player_shp"), assets::Texture::NONE);
    EXPECT_TRUE(textures->loads.empty());
    EXPECT_EQ(manager.status<assets::Texture>("player_shp"), assets::AssetStatus::Unknown);
    EXPECT_EQ(manager.warnings().size(), 1U);
}

TEST_F(AssetManagerTest, AnAliasDeclaredAgainForAnotherFileReleasesTheOldOne) {
    manager.declare<assets::Texture>("ship", "sprites/old.png", "global");
    manager.declare<assets::Texture>("ship", "sprites/old.png", "level1");
    manager.loadGroup("global");
    manager.loadGroup("level1");
    const auto old = manager.get<assets::Texture>("ship");

    manager.declare<assets::Texture>("ship", "sprites/new.png");
    EXPECT_EQ(manager.warnings().size(), 1U);
    const auto ship = manager.get<assets::Texture>("ship");
    EXPECT_NE(ship, old);
    EXPECT_EQ(manager.get<assets::Texture>("sprites/new.png"), ship);
    // Nothing needs the old file any more, in any group.
    EXPECT_EQ(textures->unloads, std::vector<std::uint32_t>{old});
    EXPECT_EQ(manager.status<assets::Texture>("sprites/old.png"), assets::AssetStatus::Unknown);
    EXPECT_EQ(manager.loadedCount<assets::Texture>(), 1U);

    // The alias kept its groups: unloading level1 keeps the ship (global), loading level1 loads nothing old.
    manager.unloadGroup("level1");
    EXPECT_EQ(manager.get<assets::Texture>("ship"), ship);
    manager.loadGroup("level1");
    EXPECT_EQ(std::ranges::count(textures->loads, "sprites/old.png"), 1);
}

TEST_F(AssetManagerTest, UndeclaringAnAliasReleasesItsFile) {
    manager.declare<assets::Texture>("ship", "sprites/ship.png");
    manager.loadGroup("global");
    EXPECT_TRUE(manager.undeclare<assets::Texture>("ship"));
    EXPECT_EQ(textures->unloads.size(), 1U);
    EXPECT_EQ(manager.status<assets::Texture>("ship"), assets::AssetStatus::Unknown);
    EXPECT_FALSE(manager.undeclare<assets::Texture>("ship"));
}

TEST_F(AssetManagerTest, ForgettingAGroupReleasesAndRemovesItsDeclarations) {
    manager.declare<assets::Texture>("boss", "sprites/boss.png", "level1");
    manager.declare<assets::Texture>("ship", "sprites/ship.png", "level1");
    manager.declare<assets::Texture>("ship", "sprites/ship.png", "global");
    manager.loadGroup("level1");
    manager.forgetGroup("level1");
    EXPECT_FALSE(manager.isGroupLoaded("level1"));
    EXPECT_EQ(manager.status<assets::Texture>("boss"), assets::AssetStatus::Unknown);
    EXPECT_EQ(manager.status<assets::Texture>("ship"), assets::AssetStatus::Declared);
    EXPECT_EQ(manager.groupProgress("level1").total, 0U);
}

TEST_F(AssetManagerTest, TheSameFileWithOtherOptionsIsAnotherAsset) {
    manager.declare<assets::Texture>("a", "a.png", "global", {.filter = assets::TextureFilter::Smooth});
    manager.declare<assets::Texture>("b", "a.png", "global", {.filter = assets::TextureFilter::Nearest});
    manager.loadGroup("global");
    ASSERT_EQ(textures->options.size(), 2U);
    EXPECT_NE(manager.get<assets::Texture>("a"), manager.get<assets::Texture>("b"));
    EXPECT_TRUE(manager.warnings().empty());
    // Declaring the same pair again loads nothing.
    manager.declare<assets::Texture>("a", "a.png", "global", {.filter = assets::TextureFilter::Smooth});
    EXPECT_EQ(textures->options.size(), 2U);
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

TEST(AssetManager, GamesAddTheirOwnKindsWithoutTouchingTheEngine) {
    assets::AssetManager manager;
    EXPECT_TRUE(manager.hasKind("texture"));
    EXPECT_FALSE(manager.hasKind("level"));
    manager.registerKind<Level>();
    EXPECT_TRUE(manager.hasKind("level"));

    auto levels = std::make_shared<LoaderLog>();
    manager.setLoader<Level>(fakeLoader<Level>(levels));
    // Reached by name, as scripts do.
    EXPECT_TRUE(manager.declare("level", "stage1", "levels/stage1.json", "global"));
    EXPECT_TRUE(manager.loadGroup("global"));
    EXPECT_EQ(levels->loads, std::vector<std::string>{"levels/stage1.json"});
    EXPECT_EQ(manager.handle("level", "stage1"), manager.get<Level>("stage1"));
    EXPECT_EQ(manager.status("level", "stage1"), assets::AssetStatus::Loaded);

    EXPECT_FALSE(manager.declare("shader", "x", "x.glsl", "global"));
    EXPECT_THROW(manager.registerKind<OtherTexture>(), std::logic_error);
}

TEST_F(AssetManagerTest, AGroupLoadsStepByStepBehindALoadingScreen) {
    for (int i = 0; i < 5; ++i) {
        manager.declare<assets::Texture>("tile_" + std::to_string(i), "tiles/" + std::to_string(i) + ".png", "level1");
    }
    assets::GroupProgress progress = manager.loadGroupStep("level1", 2);
    EXPECT_TRUE(manager.isGroupLoaded("level1"));
    EXPECT_EQ(progress.total, 5U);
    EXPECT_EQ(progress.loaded, 2U);
    EXPECT_FALSE(progress.done());
    EXPECT_FLOAT_EQ(progress.ratio(), 0.4F);

    progress = manager.loadGroupStep("level1", 2);
    EXPECT_EQ(progress.loaded, 4U);
    progress = manager.loadGroupStep("level1", 2);
    EXPECT_TRUE(progress.done());
    EXPECT_EQ(textures->loads.size(), 5U);
    EXPECT_FLOAT_EQ(manager.groupProgress("level1").ratio(), 1.0F);
}

TEST_F(AssetManagerTest, PreloadDirectoryDeclaresFilesByStemAndLoadsThemWithTheGroup) {
    const TempDirectory directory;
    directory.touch("b_ship.png");
    directory.touch("a_star.PNG");
    directory.touch("notes.txt");
    directory.touch("c_sound.wav");
    directory.touch("d.old.png");

    EXPECT_EQ(manager.preloadDirectory<assets::Texture>(directory.path(), "level1"), 2U);
    // "d.old" is not a valid alias.
    EXPECT_EQ(manager.warnings().size(), 1U);
    EXPECT_TRUE(textures->loads.empty());
    ASSERT_TRUE(manager.loadGroup("level1"));
    ASSERT_EQ(textures->loads.size(), 2U);
    EXPECT_TRUE(textures->loads[0].ends_with("a_star.PNG"));
    EXPECT_TRUE(textures->loads[1].ends_with("b_ship.png"));
    EXPECT_NE(manager.get<assets::Texture>("a_star"), assets::Texture::NONE);
    EXPECT_NE(manager.get<assets::Texture>("b_ship"), assets::Texture::NONE);
    EXPECT_EQ(manager.get<assets::Texture>("notes"), assets::Texture::NONE);

    manager.unloadGroup("level1");
    EXPECT_EQ(manager.loadedCount<assets::Texture>(), 0U);
}

TEST_F(AssetManagerTest, PreloadingIntoALoadedGroupLoadsAtOnce) {
    const TempDirectory directory;
    directory.touch("ship.png");
    manager.loadGroup("global");
    EXPECT_EQ(manager.preloadDirectory<assets::Texture>(directory.path()), 1U);
    EXPECT_EQ(textures->loads.size(), 1U);
}

TEST_F(AssetManagerTest, PreloadingAMissingDirectoryWarns) {
    EXPECT_EQ(manager.preloadDirectory<assets::Texture>("does/not/exist"), 0U);
    EXPECT_EQ(manager.warnings().size(), 1U);
}

TEST_F(AssetManagerTest, AHotReloadDropsTheDeclarationsTheScriptNoLongerMakes) {
    manager.declare<assets::Texture>("from_code", "sprites/code.png");
    manager.beginScriptDeclarations(false);
    manager.declare<assets::Texture>("kept", "sprites/kept.png");
    manager.declare<assets::Texture>("removed", "sprites/removed.png");
    manager.endScriptDeclarations();
    manager.loadGroup("global");
    ASSERT_EQ(manager.loadedCount<assets::Texture>(), 3U);

    // The script runs again without the "removed" line.
    manager.beginScriptDeclarations(true);
    manager.declare<assets::Texture>("kept", "sprites/kept.png");
    manager.endScriptDeclarations();

    EXPECT_EQ(manager.status<assets::Texture>("removed"), assets::AssetStatus::Unknown);
    EXPECT_EQ(manager.status<assets::Texture>("kept"), assets::AssetStatus::Loaded);
    EXPECT_EQ(manager.status<assets::Texture>("from_code"), assets::AssetStatus::Loaded);
    EXPECT_EQ(textures->unloads.size(), 1U);
    EXPECT_EQ(textures->loads.size(), 3U);
}

TEST_F(AssetManagerTest, FreedEntriesAreReused) {
    for (int i = 0; i < 100; ++i) {
        manager.declare<assets::Texture>("temp", "sprites/temp_" + std::to_string(i) + ".png", "scratch");
        manager.undeclare<assets::Texture>("temp");
    }
    EXPECT_EQ(manager.groupProgress("scratch").total, 0U);
    EXPECT_EQ(manager.status<assets::Texture>("sprites/temp_0.png"), assets::AssetStatus::Unknown);
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
