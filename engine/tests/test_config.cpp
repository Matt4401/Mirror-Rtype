#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "config/ConfigConstants.hpp"
#include "config/ConfigFile.hpp"
#include "config/ConfigReader.hpp"

namespace {

/** @brief Writes `content` to a file in the temporary directory and returns its path. */
std::filesystem::path writeTempFile(const std::string& name, const std::string& content) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream(path, std::ios::binary) << content;
    return path;
}

}  // namespace

TEST(ConfigFile, ReadsScalarValuesByDottedKey) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        title = "R-Type"
        [audio]
        voices = 4
        [audio.spatial]
        enabled = false
        max_pan = 0.6
    )");

    ASSERT_FALSE(file.error().has_value());
    EXPECT_EQ(file.getString("title"), "R-Type");
    EXPECT_EQ(file.getInteger("audio.voices"), 4);
    EXPECT_EQ(file.getBool("audio.spatial.enabled"), false);
    EXPECT_DOUBLE_EQ(file.getNumber("audio.spatial.max_pan").value_or(0.0), 0.6);
}

TEST(ConfigFile, MissingKeysAndWrongTypesGiveNothing) {
    const config::ConfigFile file = config::ConfigFile::parse("volume = \"loud\"");

    EXPECT_FALSE(file.contains("missing"));
    EXPECT_FALSE(file.getNumber("missing").has_value());
    EXPECT_TRUE(file.contains("volume"));
    EXPECT_FALSE(file.getNumber("volume").has_value());
    EXPECT_FALSE(file.getBool("volume").has_value());
}

TEST(ConfigFile, IntegersAreAcceptedAsNumbers) {
    const config::ConfigFile file = config::ConfigFile::parse("volume = 1");

    EXPECT_DOUBLE_EQ(file.getNumber("volume").value_or(0.0), 1.0);
    EXPECT_EQ(file.getInteger("volume"), std::int64_t{1});
}

TEST(ConfigFile, ListsTheKeysOfATable) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        [audio]
        backend = "auto"
        [audio.volume]
        sfx = 1.0
        [audiovisual]
        other = 1
    )");

    const std::vector<std::string> expected{"audio.backend", "audio.volume.sfx"};
    EXPECT_EQ(file.keysUnder("audio"), expected);
    EXPECT_TRUE(file.keysUnder("render").empty());
}

TEST(ConfigFile, ArrayElementsAreKeyedByIndex) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        heights = [160.0, 420.0, 280.0]
        names = ["a", "b"]
    )");

    ASSERT_FALSE(file.error().has_value());
    EXPECT_TRUE(file.warnings().empty());
    EXPECT_EQ(file.arraySize("heights"), 3U);
    EXPECT_DOUBLE_EQ(file.getNumber("heights.0").value_or(0.0), 160.0);
    EXPECT_DOUBLE_EQ(file.getNumber("heights.2").value_or(0.0), 280.0);
    EXPECT_EQ(file.arraySize("names"), 2U);
    EXPECT_EQ(file.getString("names.1"), "b");
    EXPECT_FALSE(file.contains("heights.3"));
}

TEST(ConfigFile, ArraysOfTablesAreKeyedByIndex) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        [[events]]
        at = 400.0
        prefab = "alien_scout"

        [[events]]
        at = 900.0
        count = 5
    )");

    ASSERT_FALSE(file.error().has_value());
    EXPECT_EQ(file.arraySize("events"), 2U);
    EXPECT_DOUBLE_EQ(file.getNumber("events.0.at").value_or(0.0), 400.0);
    EXPECT_EQ(file.getString("events.0.prefab"), "alien_scout");
    EXPECT_EQ(file.getInteger("events.1.count"), 5);
    EXPECT_FALSE(file.contains("events.1.prefab"));
}

TEST(ConfigFile, ArraysInsideTablesUseTheFullDottedKey) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        [level]
        name = "Sector 1"

        [[level.waves]]
        y = 200.0
    )");

    EXPECT_EQ(file.getString("level.name"), "Sector 1");
    EXPECT_EQ(file.arraySize("level.waves"), 1U);
    EXPECT_DOUBLE_EQ(file.getNumber("level.waves.0.y").value_or(0.0), 200.0);
}

TEST(ConfigFile, NestedArraysAreFlattened) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        path = [[0.0, 100.0], [50.0, 200.0, 300.0]]

        [[events]]
        points = [1, 2]
    )");

    EXPECT_EQ(file.arraySize("path"), 2U);
    EXPECT_EQ(file.arraySize("path.0"), 2U);
    EXPECT_EQ(file.arraySize("path.1"), 3U);
    EXPECT_DOUBLE_EQ(file.getNumber("path.1.2").value_or(0.0), 300.0);
    EXPECT_EQ(file.arraySize("events.0.points"), 2U);
    EXPECT_EQ(file.getInteger("events.0.points.1"), 2);
}

TEST(ConfigFile, EmptyArrayHasSizeZero) {
    const config::ConfigFile file = config::ConfigFile::parse("events = []");

    ASSERT_FALSE(file.error().has_value());
    EXPECT_EQ(file.arraySize("events"), 0U);
    EXPECT_TRUE(file.keysUnder("events").empty());
}

TEST(ConfigFile, ArraySizeIsZeroForMissingKeysAndScalars) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        volume = 1.0
        [audio]
        backend = "auto"
    )");

    EXPECT_EQ(file.arraySize("missing"), 0U);
    EXPECT_EQ(file.arraySize("volume"), 0U);
    EXPECT_EQ(file.arraySize("audio"), 0U);
}

TEST(ConfigFile, ArraysAreNotScalarValues) {
    const config::ConfigFile file = config::ConfigFile::parse("keys = [1, 2]");

    EXPECT_FALSE(file.contains("keys"));
    EXPECT_FALSE(file.getInteger("keys").has_value());
}

TEST(ConfigFile, MixedTypeArrayKeepsEachElementType) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(mixed = [1, "two", true, 4.5])");

    EXPECT_EQ(file.arraySize("mixed"), 4U);
    EXPECT_EQ(file.getInteger("mixed.0"), 1);
    EXPECT_EQ(file.getString("mixed.1"), "two");
    EXPECT_EQ(file.getBool("mixed.2"), true);
    EXPECT_DOUBLE_EQ(file.getNumber("mixed.3").value_or(0.0), 4.5);
}

TEST(ConfigFile, UnsupportedValuesInsideArraysAreReportedWithTheirIndex) {
    const config::ConfigFile file = config::ConfigFile::parse("dates = [1, 1987-07-01]");

    EXPECT_EQ(file.arraySize("dates"), 2U);
    EXPECT_EQ(file.getInteger("dates.0"), 1);
    EXPECT_FALSE(file.contains("dates.1"));
    ASSERT_EQ(file.warnings().size(), 1U);
    EXPECT_NE(file.warnings()[0].find("dates.1"), std::string::npos) << file.warnings()[0];
}

TEST(ConfigFile, ArrayElementsAreListedUnderTheirTable) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        [[events]]
        at = 1.0
        [[events]]
        at = 2.0
    )");

    const std::vector<std::string> expected{"events.0.at", "events.1.at"};
    EXPECT_EQ(file.keysUnder("events"), expected);
}

TEST(ConfigReader, ReadsValuesInsideArraysOfTables) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        [[events]]
        at = 400.0
        count = 500
    )");
    std::vector<std::string> warnings;
    config::ConfigReader reader(file, warnings);

    EXPECT_FLOAT_EQ(reader.number("events.0.at", 0.0F, 0.0F, 100000.0F), 400.0F);
    EXPECT_EQ(reader.count("events.0.count", 1, 1, 64), 64U);
    EXPECT_EQ(warnings.size(), 1U);
}

TEST(ConfigFile, UnsupportedValuesAreReported) {
    const config::ConfigFile file = config::ConfigFile::parse("released = 1987-07-01");

    EXPECT_FALSE(file.error().has_value());
    EXPECT_FALSE(file.contains("released"));
    ASSERT_EQ(file.warnings().size(), 1U);
    EXPECT_NE(file.warnings()[0].find("released"), std::string::npos);
}

TEST(ConfigFile, SyntaxErrorGivesAnEmptyConfigurationWithItsLocation) {
    const config::ConfigFile file = config::ConfigFile::parse("[audio]\nvolume = = 2\n", "client.toml");

    ASSERT_TRUE(file.error().has_value());
    EXPECT_NE(file.error()->find("client.toml:2"), std::string::npos) << *file.error();
    EXPECT_TRUE(file.keysUnder("audio").empty());
}

TEST(ConfigFile, LoadsAFile) {
    const config::ConfigFile file = config::ConfigFile::load(writeTempFile("squacaron_config.toml", "speed = 2.5"));

    ASSERT_FALSE(file.error().has_value());
    EXPECT_DOUBLE_EQ(file.getNumber("speed").value_or(0.0), 2.5);
}

TEST(ConfigFile, EmptyFileIsValid) {
    const config::ConfigFile file = config::ConfigFile::load(writeTempFile("squacaron_empty.toml", ""));

    EXPECT_FALSE(file.error().has_value());
}

TEST(ConfigFile, MissingFileIsReported) {
    const config::ConfigFile file = config::ConfigFile::load("does/not/exist.toml");

    ASSERT_TRUE(file.error().has_value());
    EXPECT_FALSE(file.contains("anything"));
}

TEST(ConfigFile, OversizedFileIsRejected) {
    const std::string comment(config::MAX_CONFIG_FILE_SIZE + 1, '#');
    const config::ConfigFile file = config::ConfigFile::load(writeTempFile("squacaron_huge.toml", comment));

    ASSERT_TRUE(file.error().has_value());
    EXPECT_NE(file.error()->find("too large"), std::string::npos) << *file.error();
}

TEST(ConfigReader, MissingKeysGiveTheFallbackSilently) {
    const config::ConfigFile file = config::ConfigFile::parse("");
    std::vector<std::string> warnings;
    config::ConfigReader reader(file, warnings);

    EXPECT_FLOAT_EQ(reader.number("game.speed", 2.0F, 0.0F, 10.0F), 2.0F);
    EXPECT_EQ(reader.count("game.lives", 3, 1, 9), 3U);
    EXPECT_TRUE(reader.flag("game.vsync", true));
    EXPECT_EQ(reader.text("game.name", "R-Type"), "R-Type");
    EXPECT_TRUE(warnings.empty());
}

TEST(ConfigFile, KeysFlattenedTwiceAreReported) {
    const config::ConfigFile file = config::ConfigFile::parse(R"(
        keys = [10]
        "keys.0" = 20
    )");

    ASSERT_EQ(file.warnings().size(), 1U);
    EXPECT_NE(file.warnings()[0].find("keys.0: defined twice"), std::string::npos) << file.warnings()[0];
    EXPECT_TRUE(file.getInteger("keys.0").has_value());
}

TEST(ConfigReader, ReadsValidValues) {
    const config::ConfigFile file =
        config::ConfigFile::parse("[game]\nspeed = 4\nlives = 5\nvsync = false\nname = \"Demo\"\n");
    std::vector<std::string> warnings;
    config::ConfigReader reader(file, warnings);

    EXPECT_FLOAT_EQ(reader.number("game.speed", 2.0F, 0.0F, 10.0F), 4.0F);
    EXPECT_EQ(reader.count("game.lives", 3, 1, 9), 5U);
    EXPECT_FALSE(reader.flag("game.vsync", true));
    EXPECT_EQ(reader.text("game.name", "R-Type"), "Demo");
    EXPECT_TRUE(warnings.empty());
}

TEST(ConfigReader, InvalidValuesAreCorrectedAndReported) {
    const config::ConfigFile file =
        config::ConfigFile::parse("[game]\nspeed = 99.0\nlives = -4\nvsync = \"yes\"\nname = 7\ngain = nan\n");
    std::vector<std::string> warnings;
    config::ConfigReader reader(file, warnings);

    EXPECT_FLOAT_EQ(reader.number("game.speed", 2.0F, 0.0F, 10.0F), 10.0F);
    EXPECT_EQ(reader.count("game.lives", 3, 1, 9), 1U);
    EXPECT_TRUE(reader.flag("game.vsync", true));
    EXPECT_EQ(reader.text("game.name", "R-Type"), "R-Type");
    EXPECT_FLOAT_EQ(reader.number("game.gain", 1.0F, 0.0F, 1.0F), 1.0F);
    EXPECT_EQ(warnings.size(), 5U);
}

TEST(ConfigReader, ReportsKeysThatWereNeverRead) {
    const config::ConfigFile file = config::ConfigFile::parse("[game]\nspeed = 4\nsped = 5\n[other]\nkey = 1\n");
    std::vector<std::string> warnings;
    config::ConfigReader reader(file, warnings);

    (void)reader.number("game.speed", 2.0F, 0.0F, 10.0F);
    reader.reportUnknownKeys("game");

    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_NE(warnings[0].find("game.sped"), std::string::npos) << warnings[0];
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
