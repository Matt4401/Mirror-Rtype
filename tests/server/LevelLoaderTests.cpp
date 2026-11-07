#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "config/ConfigFile.hpp"
#include "level/Level.hpp"
#include "level/LevelLoader.hpp"
#include "timeline/TimelineEvent.hpp"

namespace {

namespace level = rtype::server::level;

bool knownPrefab(std::string_view name) { return name == "alien_scout" || name == "obstacle"; }

level::Level build(std::string_view toml, std::vector<std::string>& warnings) {
    return level::buildLevel(config::ConfigFile::parse(toml), knownPrefab, warnings);
}

level::Level build(std::string_view toml) {
    std::vector<std::string> warnings;
    return build(toml, warnings);
}

const timeline::TimelineEvent& eventAt(const level::Level& built, std::size_t index) {
    return built.timeline.events()[index];
}

bool anyWarningMentions(const std::vector<std::string>& warnings, std::string_view text) {
    for (const std::string& warning : warnings) {
        if (warning.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(LevelLoader, ReadsTheLevelSettings) {
    const level::Level built = build(R"(
        [level]
        name = "Sector 1"
        scroll_speed = 80.0
    )");

    EXPECT_EQ(built.name, "Sector 1");
    EXPECT_FLOAT_EQ(built.scrollSpeed, 80.0F);
    EXPECT_TRUE(built.timeline.events().empty());
}

TEST(LevelLoader, MissingSettingsUseTheDefaults) {
    std::vector<std::string> warnings;
    const level::Level built = build("", warnings);

    EXPECT_EQ(built.name, "Unnamed level");
    EXPECT_FLOAT_EQ(built.scrollSpeed, level::DEFAULT_SCROLL_SPEED);
    EXPECT_TRUE(warnings.empty());
}

TEST(LevelLoader, ReadsASpawnEvent) {
    const level::Level built = build(R"(
        [[events]]
        at = 400.0
        type = "spawn"
        prefab = "alien_scout"
        x = 1300.0
        y = 200.0
    )");

    ASSERT_EQ(built.timeline.events().size(), 1U);
    const timeline::TimelineEvent& spawn = eventAt(built, 0);
    EXPECT_FLOAT_EQ(spawn.position, 400.0F);
    EXPECT_EQ(spawn.type, level::EVENT_SPAWN);
    EXPECT_EQ(spawn.params.find<std::string>("prefab"), "alien_scout");
    EXPECT_DOUBLE_EQ(spawn.params.find<double>("x").value_or(0.0), 1300.0);
    EXPECT_DOUBLE_EQ(spawn.params.find<double>("y").value_or(0.0), 200.0);
}

TEST(LevelLoader, SpawnWithoutPositionUsesTheDefaultSpawnPoint) {
    const level::Level built = build(R"(
        [[events]]
        at = 0.0
        type = "spawn"
        prefab = "alien_scout"
    )");

    ASSERT_EQ(built.timeline.events().size(), 1U);
    EXPECT_DOUBLE_EQ(eventAt(built, 0).params.find<double>("x").value_or(0.0), level::DEFAULT_SPAWN_X);
    EXPECT_DOUBLE_EQ(eventAt(built, 0).params.find<double>("y").value_or(0.0), level::DEFAULT_SPAWN_Y);
}

TEST(LevelLoader, WaveIsExpandedIntoOneSpawnPerEnemy) {
    const level::Level built = build(R"(
        [level]
        scroll_speed = 100.0

        [[events]]
        at = 1000.0
        type = "spawn"
        prefab = "alien_scout"
        count = 3
        interval = 0.5
        y = 200.0
        spacing_y = 50.0
    )");

    ASSERT_EQ(built.timeline.events().size(), 3U);
    for (std::size_t i = 0; i < 3; ++i) {
        const timeline::TimelineEvent& spawn = eventAt(built, i);
        // 0.5 s apart at 100 px/s: 50 px of scroll between two enemies.
        EXPECT_FLOAT_EQ(spawn.position, 1000.0F + (50.0F * static_cast<float>(i)));
        EXPECT_DOUBLE_EQ(spawn.params.find<double>("y").value_or(0.0), 200.0 + (50.0 * static_cast<double>(i)));
    }
}

TEST(LevelLoader, WaveIsSpacedAtTheScrollSpeedWhereItStarts) {
    const level::Level built = build(R"(
        [level]
        scroll_speed = 100.0

        [[events]]
        at = 500.0
        type = "scroll"
        speed = 20.0

        [[events]]
        at = 600.0
        type = "spawn"
        prefab = "alien_scout"
        count = 2
        interval = 1.0
    )");

    ASSERT_EQ(built.timeline.events().size(), 3U);
    EXPECT_FLOAT_EQ(eventAt(built, 1).position, 600.0F);
    EXPECT_FLOAT_EQ(eventAt(built, 2).position, 620.0F);
}

TEST(LevelLoader, ExtraSpawnKeysArePassedToThePrefab) {
    const level::Level built = build(R"(
        [[events]]
        at = 0.0
        type = "spawn"
        prefab = "alien_scout"
        pattern = "sine"
        amplitude = 40.0
        speed = -200.0
        boss = false
    )");

    ASSERT_EQ(built.timeline.events().size(), 1U);
    const timeline::TimelineEvent& spawn = eventAt(built, 0);
    EXPECT_EQ(spawn.params.find<std::string>("pattern"), "sine");
    EXPECT_DOUBLE_EQ(spawn.params.find<double>("amplitude").value_or(0.0), 40.0);
    EXPECT_DOUBLE_EQ(spawn.params.find<double>("speed").value_or(0.0), -200.0);
    EXPECT_EQ(spawn.params.find<bool>("boss"), false);
    EXPECT_FALSE(spawn.params.has("at"));
    EXPECT_FALSE(spawn.params.has("count"));
}

TEST(LevelLoader, ReadsScrollAndEndEvents) {
    const level::Level built = build(R"(
        [[events]]
        at = 2400.0
        type = "scroll"
        speed = 30.0

        [[events]]
        at = 6000.0
        type = "end"
    )");

    ASSERT_EQ(built.timeline.events().size(), 2U);
    EXPECT_EQ(eventAt(built, 0).type, level::EVENT_SCROLL);
    EXPECT_DOUBLE_EQ(eventAt(built, 0).params.find<double>("speed").value_or(0.0), 30.0);
    EXPECT_EQ(eventAt(built, 1).type, level::EVENT_END);
    EXPECT_FLOAT_EQ(eventAt(built, 1).position, 6000.0F);
}

TEST(LevelLoader, EventsAreSortedByPositionWhateverTheFileOrder) {
    const level::Level built = build(R"(
        [[events]]
        at = 900.0
        type = "end"

        [[events]]
        at = 100.0
        type = "spawn"
        prefab = "obstacle"
    )");

    ASSERT_EQ(built.timeline.events().size(), 2U);
    EXPECT_EQ(eventAt(built, 0).type, level::EVENT_SPAWN);
    EXPECT_EQ(eventAt(built, 1).type, level::EVENT_END);
}

TEST(LevelLoader, UnknownPrefabIsSkippedWithAWarning) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[events]]
        at = 0.0
        type = "spawn"
        prefab = "dragon"
    )",
                                     warnings);

    EXPECT_TRUE(built.timeline.events().empty());
    EXPECT_TRUE(anyWarningMentions(warnings, "dragon"));
}

TEST(LevelLoader, InvalidEventsAreSkippedWithAWarning) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[events]]
        type = "spawn"
        prefab = "alien_scout"

        [[events]]
        at = 10.0

        [[events]]
        at = 20.0
        type = "teleport"

        [[events]]
        at = 30.0
        type = "spawn"

        [[events]]
        at = 40.0
        type = "scroll"

        [[events]]
        at = "soon"
        type = "end"
    )",
                                     warnings);

    EXPECT_TRUE(built.timeline.events().empty());
    EXPECT_TRUE(anyWarningMentions(warnings, "events.0"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.1"));
    EXPECT_TRUE(anyWarningMentions(warnings, "teleport"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.3"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.4"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.5"));
}

TEST(LevelLoader, OutOfRangeValuesAreClampedWithAWarning) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [level]
        scroll_speed = 99999.0

        [[events]]
        at = -50.0
        type = "spawn"
        prefab = "alien_scout"
        count = 1000
        interval = 0.0
    )",
                                     warnings);

    EXPECT_FLOAT_EQ(built.scrollSpeed, level::MAX_SCROLL_SPEED);
    ASSERT_EQ(built.timeline.events().size(), level::MAX_WAVE_COUNT);
    EXPECT_FLOAT_EQ(eventAt(built, 0).position, 0.0F);
    EXPECT_TRUE(anyWarningMentions(warnings, "level.scroll_speed"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.0.at"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.0.count"));
}

TEST(LevelLoader, TooManyEventsAreTruncated) {
    std::string toml;
    for (std::size_t i = 0; i < level::MAX_EVENTS + 10; ++i) {
        toml += "[[events]]\nat = 1.0\ntype = \"end\"\n";
    }
    std::vector<std::string> warnings;
    const level::Level built = build(toml, warnings);

    EXPECT_EQ(built.timeline.events().size(), level::MAX_EVENTS);
    EXPECT_TRUE(anyWarningMentions(warnings, "only the first"));
}

TEST(LevelLoader, UnknownLevelSettingsAreReported) {
    std::vector<std::string> warnings;
    (void)build(R"(
        [level]
        name = "Sector 1"
        music = "boss.ogg"
    )",
                warnings);

    EXPECT_TRUE(anyWarningMentions(warnings, "level.music"));
}

TEST(LevelLoader, LoadsALevelFile) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "rtype_level_test.toml";
    std::ofstream(path) << "[level]\nname = \"From disk\"\n[[events]]\nat = 5.0\ntype = \"end\"\n";
    std::vector<std::string> warnings;

    const auto loaded = level::loadLevel(path, knownPrefab, warnings);

    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    EXPECT_EQ(loaded->name, "From disk");
    EXPECT_EQ(loaded->timeline.events().size(), 1U);
    std::filesystem::remove(path);
}

TEST(LevelLoader, MissingFileIsAnError) {
    std::vector<std::string> warnings;

    const auto loaded = level::loadLevel("does/not/exist.toml", knownPrefab, warnings);

    ASSERT_FALSE(loaded.has_value());
    EXPECT_NE(loaded.error().find("exist.toml"), std::string::npos);
}

TEST(LevelLoader, MalformedFileIsAnError) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "rtype_level_broken.toml";
    std::ofstream(path) << "[[events]\nat = = 1\n";
    std::vector<std::string> warnings;

    const auto loaded = level::loadLevel(path, knownPrefab, warnings);

    EXPECT_FALSE(loaded.has_value());
    std::filesystem::remove(path);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
