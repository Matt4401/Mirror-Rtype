#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "config/ConfigConstants.hpp"
#include "config/ConfigFile.hpp"

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

TEST(ConfigFile, UnsupportedValuesAreReported) {
    const config::ConfigFile file = config::ConfigFile::parse("keys = [1, 2, 3]");

    EXPECT_FALSE(file.error().has_value());
    EXPECT_FALSE(file.contains("keys"));
    ASSERT_EQ(file.warnings().size(), 1U);
    EXPECT_NE(file.warnings()[0].find("keys"), std::string::npos);
}

TEST(ConfigFile, SyntaxErrorGivesAnEmptyConfigurationWithItsLocation) {
    const config::ConfigFile file = config::ConfigFile::parse("[audio]\nvolume = = 2\n", "client.toml");

    ASSERT_TRUE(file.error().has_value());
    EXPECT_NE(file.error()->find("client.toml:2"), std::string::npos) << *file.error();
    EXPECT_TRUE(file.keysUnder("audio").empty());
}

TEST(ConfigFile, LoadsAFile) {
    const config::ConfigFile file = config::ConfigFile::load(writeTempFile("mirror_rtype_config.toml", "speed = 2.5"));

    ASSERT_FALSE(file.error().has_value());
    EXPECT_DOUBLE_EQ(file.getNumber("speed").value_or(0.0), 2.5);
}

TEST(ConfigFile, EmptyFileIsValid) {
    const config::ConfigFile file = config::ConfigFile::load(writeTempFile("mirror_rtype_empty.toml", ""));

    EXPECT_FALSE(file.error().has_value());
}

TEST(ConfigFile, MissingFileIsReported) {
    const config::ConfigFile file = config::ConfigFile::load("does/not/exist.toml");

    ASSERT_TRUE(file.error().has_value());
    EXPECT_FALSE(file.contains("anything"));
}

TEST(ConfigFile, OversizedFileIsRejected) {
    const std::string comment(config::MAX_CONFIG_FILE_SIZE + 1, '#');
    const config::ConfigFile file = config::ConfigFile::load(writeTempFile("mirror_rtype_huge.toml", comment));

    ASSERT_TRUE(file.error().has_value());
    EXPECT_NE(file.error()->find("too large"), std::string::npos) << *file.error();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
