#include <gtest/gtest.h>

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "parsing/Parsing.hpp"
#include "protocol/Messages.hpp"

namespace {

using rtype::server::Options;
using rtype::server::parseOptions;

/** @brief Parses a command line given as strings, program name first. */
std::optional<Options> parse(std::initializer_list<std::string> words) {
    std::vector<std::string> storage(words);
    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (std::string& word : storage) {
        argv.push_back(word.data());
    }
    return parseOptions(argv);
}

}  // namespace

TEST(ServerOptions, DefaultsWithoutArguments) {
    const auto options = parse({"r-type_server"});
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->port, rtype::protocol::DEFAULT_PORT);
    EXPECT_FALSE(options->logs);
    EXPECT_FALSE(options->level.has_value());
}

TEST(ServerOptions, ReadsTheLevelPath) {
    const auto options = parse({"r-type_server", "--level", "/tmp/test level.toml"});
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->level, "/tmp/test level.toml");
}

TEST(ServerOptions, LevelMixesWithTheOtherArguments) {
    const auto options = parse({"r-type_server", "5000", "--level", "levels/a.toml", "--logs-colorful"});
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->port, 5000);
    EXPECT_TRUE(options->colors);
    EXPECT_EQ(options->level, "levels/a.toml");
}

TEST(ServerOptions, LevelWithoutPathIsRejected) {
    EXPECT_FALSE(parse({"r-type_server", "--level"}).has_value());
    EXPECT_FALSE(parse({"r-type_server", "--level", ""}).has_value());
}

TEST(ServerOptions, LevelGivenTwiceIsRejected) {
    EXPECT_FALSE(parse({"r-type_server", "--level", "a.toml", "--level", "b.toml"}).has_value());
}

TEST(ServerOptions, PortAndLogsStillWork) {
    const auto options = parse({"r-type_server", "--logs", "4300"});
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->port, 4300);
    EXPECT_TRUE(options->logs);
    EXPECT_FALSE(options->colors);
    EXPECT_FALSE(parse({"r-type_server", "--logs", "--logs-colorful"}).has_value());
    EXPECT_FALSE(parse({"r-type_server", "0"}).has_value());
    EXPECT_FALSE(parse({"r-type_server", "--unknown"}).has_value());
}

TEST(ServerOptions, UsageMentionsTheLevelOption) {
    EXPECT_NE(rtype::server::usage("r-type_server").find("--level <path>"), std::string::npos);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
