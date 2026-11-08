#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/ConfigFile.hpp"
#include "game/ReplicationCodec.hpp"
#include "level/Level.hpp"
#include "level/LevelLoader.hpp"
#include "timeline/TimelineEvent.hpp"

namespace {

namespace level = rtype::server::level;

/**
 * @brief Test catalog: spawns of `alien_scout` or `obstacle`, bosses of `boss_core`. Their parameters are made up for
 * the tests: a movement `pattern`, its `amplitude`, a `boss` flag, and the boss `health`.
 */
std::optional<std::vector<std::string>> knownPrefab(std::string_view type, std::string_view name) {
    const bool spawn = type == level::EVENT_SPAWN && (name == "alien_scout" || name == "obstacle");
    const bool boss = type == level::EVENT_BOSS && name == "boss_core";
    if (spawn || boss) {
        return std::vector<std::string>{"speed", "pattern", "amplitude", "boss", "health"};
    }
    return std::nullopt;
}

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

TEST(LevelLoader, WaveFollowsTheScrollEventsInItsMiddle) {
    const level::Level built = build(R"(
        [level]
        scroll_speed = 100.0

        [[events]]
        at = 0.0
        type = "spawn"
        prefab = "alien_scout"
        count = 4
        interval = 1.0

        [[events]]
        at = 150.0
        type = "scroll"
        speed = 20.0
    )");

    // 1 s at 100 px/s; then 0.5 s at 100 px/s and 0.5 s at 20 px/s; then 1 s at 20 px/s, twice.
    std::vector<float> spawns;
    for (const timeline::TimelineEvent& event : built.timeline.events()) {
        if (event.type == level::EVENT_SPAWN) {
            spawns.push_back(event.position);
        }
    }
    ASSERT_EQ(spawns.size(), 4U);
    EXPECT_FLOAT_EQ(spawns.at(0), 0.0F);
    EXPECT_FLOAT_EQ(spawns.at(1), 100.0F);
    EXPECT_FLOAT_EQ(spawns.at(2), 160.0F);
    EXPECT_FLOAT_EQ(spawns.at(3), 180.0F);
}

TEST(LevelLoader, WaveDeclaredBeforeAScrollStillSeesIt) {
    // The wave comes first in the file, the scroll event at its start comes after: the order of the file does not
    // matter.
    const level::Level built = build(R"(
        [level]
        scroll_speed = 100.0

        [[events]]
        at = 200.0
        type = "spawn"
        prefab = "alien_scout"
        count = 2
        interval = 1.0

        [[events]]
        at = 200.0
        type = "scroll"
        speed = 30.0
    )");

    std::vector<float> spawns;
    for (const timeline::TimelineEvent& event : built.timeline.events()) {
        if (event.type == level::EVENT_SPAWN) {
            spawns.push_back(event.position);
        }
    }
    ASSERT_EQ(spawns.size(), 2U);
    EXPECT_FLOAT_EQ(spawns.at(0), 200.0F);
    EXPECT_FLOAT_EQ(spawns.at(1), 230.0F);
}

TEST(LevelLoader, PrefabParametersArePassedToThePrefab) {
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

TEST(LevelLoader, EachEventTypeOnlyTakesItsPrefabs) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[events]]
        at = 0.0
        type = "spawn"
        prefab = "boss_core"

        [[events]]
        at = 10.0
        type = "boss"
        prefab = "alien_scout"
    )",
                                     warnings);

    EXPECT_TRUE(built.timeline.events().empty());
    EXPECT_TRUE(anyWarningMentions(warnings, "events.0: prefab 'boss_core' cannot be used by a spawn event"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.1: prefab 'alien_scout' cannot be used by a boss event"));
}

TEST(LevelLoader, TyposInEventKeysAreReported) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[events]]
        at = 0.0
        type = "spawn"
        prefab = "alien_scout"
        cout = 5
        intervall = 0.4

        [[events]]
        at = 10.0
        type = "scroll"
        speed = 30.0
        sped = 40.0

        [[events]]
        at = 20.0
        type = "end"
        when = "now"
    )",
                                     warnings);

    ASSERT_EQ(built.timeline.events().size(), 3U);
    EXPECT_FALSE(eventAt(built, 0).params.has("cout"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.0.cout: not a parameter of 'alien_scout'"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.0.intervall: not a parameter of 'alien_scout'"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.1.sped: unknown key"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.2.when: unknown key"));
}

TEST(LevelLoader, BossClassIsKept) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[events]]
        at = 0.0
        type = "boss"
        prefab = "boss_core"
        script = "scripts/Boss.wren"
        class = "CoreBoss"
    )",
                                     warnings);

    ASSERT_EQ(built.timeline.events().size(), 1U);
    EXPECT_EQ(eventAt(built, 0).params.find<std::string>("class"), "CoreBoss");
    EXPECT_TRUE(warnings.empty());
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

TEST(LevelLoader, NonFinitePositionsSkipTheirEvent) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[events]]
        at = nan
        type = "end"

        [[events]]
        at = inf
        type = "boss"
        prefab = "boss_core"

        [[events]]
        at = -inf
        type = "spawn"
        prefab = "alien_scout"

        [[terrain]]
        from = nan
        to = 100.0
        side = "top"

        [[terrain]]
        from = 0.0
        to = inf
        side = "bottom"
    )",
                                     warnings);

    EXPECT_TRUE(built.timeline.events().empty());
    for (std::string_view key : {"events.0", "events.1", "events.2", "terrain.0", "terrain.1"}) {
        EXPECT_TRUE(anyWarningMentions(warnings, key)) << key;
    }
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

TEST(LevelLoader, ScrollSpeedCannotStopTheLevel) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [level]
        scroll_speed = 0.0

        [[events]]
        at = 100.0
        type = "scroll"
        speed = 0.0
    )",
                                     warnings);

    EXPECT_FLOAT_EQ(built.scrollSpeed, level::MIN_SCROLL_SPEED);
    ASSERT_EQ(built.timeline.events().size(), 1U);
    EXPECT_DOUBLE_EQ(eventAt(built, 0).params.find<double>("speed").value_or(0.0), level::MIN_SCROLL_SPEED);
    EXPECT_TRUE(anyWarningMentions(warnings, "level.scroll_speed"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.0.speed"));
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

TEST(LevelLoader, ReadsABossEvent) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[events]]
        at = 6000.0
        type = "boss"
        prefab = "boss_core"
        script = "scripts/Boss.wren"
        y = 300.0
        health = 40
    )",
                                     warnings);

    ASSERT_EQ(built.timeline.events().size(), 1U);
    const timeline::TimelineEvent& boss = eventAt(built, 0);
    EXPECT_FLOAT_EQ(boss.position, 6000.0F);
    EXPECT_EQ(boss.type, level::EVENT_BOSS);
    EXPECT_EQ(boss.params.find<std::string>("prefab"), "boss_core");
    EXPECT_EQ(boss.params.find<std::string>("script"), "scripts/Boss.wren");
    EXPECT_DOUBLE_EQ(boss.params.find<double>("x").value_or(0.0), level::DEFAULT_SPAWN_X);
    EXPECT_DOUBLE_EQ(boss.params.find<double>("y").value_or(0.0), 300.0);
    EXPECT_TRUE(boss.params.find<std::int64_t>("health").has_value());
    EXPECT_TRUE(warnings.empty());
}

TEST(LevelLoader, BossWithoutScriptIsKept) {
    const level::Level built = build(R"(
        [[events]]
        at = 10.0
        type = "boss"
        prefab = "boss_core"
    )");

    ASSERT_EQ(built.timeline.events().size(), 1U);
    EXPECT_FALSE(eventAt(built, 0).params.has("script"));
}

TEST(LevelLoader, BossWithoutKnownPrefabIsSkipped) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[events]]
        at = 10.0
        type = "boss"
        script = "scripts/Boss.wren"

        [[events]]
        at = 20.0
        type = "boss"
        prefab = "dragon"
    )",
                                     warnings);

    EXPECT_TRUE(built.timeline.events().empty());
    EXPECT_TRUE(anyWarningMentions(warnings, "events.0"));
    EXPECT_TRUE(anyWarningMentions(warnings, "dragon"));
}

class BossScriptPath : public ::testing::TestWithParam<std::string_view> {};

TEST_P(BossScriptPath, UnsafeScriptIsIgnoredButTheBossIsKept) {
    std::vector<std::string> warnings;
    const std::string toml = std::string(R"(
        [[events]]
        at = 10.0
        type = "boss"
        prefab = "boss_core"
        script = )") + std::string(GetParam()) +
                             "\n";
    const level::Level built = build(toml, warnings);

    ASSERT_EQ(built.timeline.events().size(), 1U);
    EXPECT_FALSE(eventAt(built, 0).params.has("script"));
    EXPECT_TRUE(anyWarningMentions(warnings, "events.0: 'script'"));
}

INSTANTIATE_TEST_SUITE_P(LevelLoader, BossScriptPath,
                         ::testing::Values(R"("/etc/passwd.wren")", R"("../secret/Boss.wren")",
                                           R"("scripts/../../Boss.wren")", R"("scripts\\Boss.wren")",
                                           R"("scripts/Boss.lua")", R"(".wren")", R"("")", "42"));

TEST(LevelLoader, ReadsTerrainSections) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[terrain]]
        from = 1800.0
        to = 3200.0
        side = "bottom"
        height = 120.0

        [[terrain]]
        from = 500.0
        to = 900.0
        side = "top"
    )",
                                     warnings);

    ASSERT_EQ(built.timeline.events().size(), 2U);
    const timeline::TimelineEvent& top = eventAt(built, 0);
    EXPECT_FLOAT_EQ(top.position, 500.0F);
    EXPECT_EQ(top.type, level::EVENT_TERRAIN);
    EXPECT_EQ(top.params.find<std::string>("side"), level::TERRAIN_TOP);
    EXPECT_DOUBLE_EQ(top.params.find<double>("height").value_or(0.0), level::DEFAULT_TERRAIN_HEIGHT);
    // Rounded up to whole terrain blocks.
    EXPECT_DOUBLE_EQ(top.params.find<double>("length").value_or(0.0), 2.0 * rtype::game::TERRAIN_BLOCK_WIDTH);

    const timeline::TimelineEvent& bottom = eventAt(built, 1);
    EXPECT_FLOAT_EQ(bottom.position, 1800.0F);
    EXPECT_EQ(bottom.params.find<std::string>("side"), level::TERRAIN_BOTTOM);
    EXPECT_DOUBLE_EQ(bottom.params.find<double>("height").value_or(0.0), 120.0);
    EXPECT_DOUBLE_EQ(bottom.params.find<double>("length").value_or(0.0), 6.0 * rtype::game::TERRAIN_BLOCK_WIDTH);
    EXPECT_TRUE(warnings.empty());
}

TEST(LevelLoader, InvalidTerrainSectionsAreSkippedWithAWarning) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[terrain]]
        to = 100.0
        side = "bottom"

        [[terrain]]
        from = 300.0
        to = 200.0
        side = "bottom"

        [[terrain]]
        from = 300.0
        to = 300.0
        side = "top"

        [[terrain]]
        from = 0.0
        to = 100.0
        side = "left"

        [[terrain]]
        from = 0.0
        to = 100.0
    )",
                                     warnings);

    EXPECT_TRUE(built.timeline.events().empty());
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_TRUE(anyWarningMentions(warnings, "terrain." + std::to_string(i))) << "section " << i;
    }
}

TEST(LevelLoader, TerrainHeightIsClampedAndTyposAreReported) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[terrain]]
        from = 0.0
        to = 100.0
        side = "bottom"
        height = 5000.0
        heigth = 10.0
    )",
                                     warnings);

    ASSERT_EQ(built.timeline.events().size(), 1U);
    EXPECT_DOUBLE_EQ(eventAt(built, 0).params.find<double>("height").value_or(0.0), level::MAX_TERRAIN_HEIGHT);
    EXPECT_TRUE(anyWarningMentions(warnings, "terrain.0.height"));
    EXPECT_TRUE(anyWarningMentions(warnings, "terrain.0.heigth"));
}

TEST(LevelLoader, OverlappingSectionsOnTheSameSideAreSkipped) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[terrain]]
        from = 0.0
        to = 512.0
        side = "bottom"

        [[terrain]]
        from = 400.0
        to = 900.0
        side = "bottom"

        [[terrain]]
        from = 512.0
        to = 900.0
        side = "bottom"
    )",
                                     warnings);

    ASSERT_EQ(built.timeline.events().size(), 2U) << "touching sections are fine, overlapping ones are not";
    EXPECT_TRUE(anyWarningMentions(warnings, "terrain.1: overlaps another section on the same side"));
}

TEST(LevelLoader, SectionsOverlapUpToTheirLastBlock) {
    std::vector<std::string> warnings;
    // The first section ends at 300 in the file, but its second block reaches 512.
    const level::Level built = build(R"(
        [[terrain]]
        from = 0.0
        to = 300.0
        side = "bottom"

        [[terrain]]
        from = 400.0
        to = 900.0
        side = "bottom"
    )",
                                     warnings);

    EXPECT_EQ(built.timeline.events().size(), 1U);
    EXPECT_TRUE(anyWarningMentions(warnings, "terrain.1: overlaps another section on the same side"));
}

TEST(LevelLoader, TopAndBottomSectionsAlwaysLeaveACorridor) {
    std::vector<std::string> warnings;
    const level::Level built = build(R"(
        [[terrain]]
        from = 0.0
        to = 500.0
        side = "top"
        height = 360.0

        [[terrain]]
        from = 200.0
        to = 700.0
        side = "bottom"
        height = 360.0
    )",
                                     warnings);

    ASSERT_EQ(built.timeline.events().size(), 2U);
    const double top = eventAt(built, 0).params.find<double>("height").value_or(0.0);
    const double bottom = eventAt(built, 1).params.find<double>("height").value_or(0.0);
    EXPECT_LE(top + bottom, level::TERRAIN_SCENE_HEIGHT - level::MIN_TERRAIN_CORRIDOR);
    EXPECT_TRUE(anyWarningMentions(warnings, "terrain.1: lowered"));
}

TEST(LevelLoader, TooManyTerrainSectionsAreTruncated) {
    std::string toml;
    for (std::size_t i = 0; i < level::MAX_TERRAIN_SECTIONS + 10; ++i) {
        // Sections one after the other: overlapping ones would be skipped.
        toml += std::format("[[terrain]]\nfrom = {}.0\nto = {}.0\nside = \"top\"\n", i * 256, (i * 256) + 256);
    }
    std::vector<std::string> warnings;
    const level::Level built = build(toml, warnings);

    EXPECT_EQ(built.timeline.events().size(), level::MAX_TERRAIN_SECTIONS);
    EXPECT_TRUE(anyWarningMentions(warnings, "terrain: "));
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

TEST(ShippedLevels, Level01LoadsWithoutWarnings) {
    std::vector<std::string> warnings;

    const auto loaded =
        level::loadLevel(std::filesystem::path(RTYPE_LEVELS_DIR) / "level_01.toml", knownPrefab, warnings);

    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    EXPECT_TRUE(warnings.empty()) << warnings.front();
    EXPECT_FALSE(loaded->timeline.events().empty());
    EXPECT_EQ(loaded->timeline.events().back().type, level::EVENT_END);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
