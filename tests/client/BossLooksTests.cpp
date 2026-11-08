#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "config/ConfigFile.hpp"
#include "looks/BossLooks.hpp"

namespace {

namespace looks = client::looks;

looks::BossLooks read(std::string_view toml, std::vector<std::string>& warnings) {
    return looks::readBossLooks(config::ConfigFile::parse(toml), warnings);
}

bool anyWarningMentions(const std::vector<std::string>& warnings, std::string_view text) {
    for (const std::string& warning : warnings) {
        if (warning.contains(text)) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(BossLooks, ReadsBoxesAndSprites) {
    std::vector<std::string> warnings;
    const looks::BossLooks result = read(R"(
        [[boss]]
        variant = 0
        width = 200.0
        height = 100.0

        [[boss]]
        variant = 3
        texture = "sprites/boss.png"
        frame_x = 10
        frame_y = 20
        frame_width = 64
        frame_height = 48
        frames = 4
        frame_duration = 0.2
        loop = false
        scale = 2.5

        [[part]]
        look = 1
        width = 30.0
    )",
                                         warnings);

    EXPECT_TRUE(warnings.empty());
    ASSERT_EQ(result.bosses.size(), 2U);
    EXPECT_FLOAT_EQ(result.bosses.at(0).width, 200.0F);
    EXPECT_FALSE(result.bosses.at(0).sprite.has_value());
    const auto& sprite = result.bosses.at(3).sprite;
    ASSERT_TRUE(sprite.has_value());
    EXPECT_EQ(sprite->texture, "sprites/boss.png");
    EXPECT_EQ(sprite->x, 10);
    EXPECT_EQ(sprite->y, 20);
    EXPECT_EQ(sprite->frameWidth, 64);
    EXPECT_EQ(sprite->frameHeight, 48);
    EXPECT_EQ(sprite->frames, 4);
    EXPECT_FLOAT_EQ(sprite->frameDuration, 0.2F);
    EXPECT_FALSE(sprite->loop);
    EXPECT_FLOAT_EQ(sprite->scale, 2.5F);
    ASSERT_EQ(result.parts.size(), 1U);
    EXPECT_FLOAT_EQ(result.parts.at(1).width, 30.0F);
}

TEST(BossLooks, UnsafeTexturesAndIncompleteSpritesAreIgnored) {
    std::vector<std::string> warnings;
    const looks::BossLooks result = read(R"(
        [[boss]]
        variant = 1
        texture = "../secret.png"
        frame_width = 10
        frame_height = 10

        [[boss]]
        variant = 2
        texture = "sprites/boss.png"
        frames = 3
    )",
                                         warnings);

    EXPECT_FALSE(result.bosses.at(1).sprite.has_value());
    EXPECT_FALSE(result.bosses.at(2).sprite.has_value());
    EXPECT_TRUE(anyWarningMentions(warnings, "boss.0.texture: texture must be a relative path"));
    EXPECT_TRUE(anyWarningMentions(warnings, "boss.1: a sprite needs frame_width and frame_height"));
    EXPECT_FALSE(anyWarningMentions(warnings, "unknown")) << "the keys of an ignored sprite are not typos";
}

TEST(BossLooks, EntriesWithoutIdAndTyposAreReported) {
    std::vector<std::string> warnings;
    const looks::BossLooks result = read(R"(
        [[boss]]
        width = 10.0

        [[part]]
        look = 300
        heigth = 20.0
    )",
                                         warnings);

    EXPECT_TRUE(result.bosses.empty());
    EXPECT_TRUE(anyWarningMentions(warnings, "boss.0: missing or invalid 'variant'"));
    EXPECT_TRUE(anyWarningMentions(warnings, "part.0.heigth: unknown setting"));
    ASSERT_EQ(result.parts.size(), 1U);
    EXPECT_TRUE(result.parts.contains(255)) << "an id is clamped to a byte";
}

TEST(BossLooks, ValuesAreClamped) {
    std::vector<std::string> warnings;
    const looks::BossLooks result = read(R"(
        [[boss]]
        variant = 0
        width = 99999.0
        texture = "sprites/boss.png"
        frame_width = 999999
        frame_height = 8
        frames = 1000
        scale = 0.0
    )",
                                         warnings);

    const looks::Look& look = result.bosses.at(0);
    EXPECT_FLOAT_EQ(look.width, looks::MAX_BOX_SIZE);
    ASSERT_TRUE(look.sprite.has_value());
    EXPECT_EQ(look.sprite->frameWidth, looks::MAX_FRAME_PIXELS);
    EXPECT_EQ(look.sprite->frames, looks::MAX_FRAMES);
    EXPECT_FLOAT_EQ(look.sprite->scale, looks::MIN_SCALE);
}

TEST(BossLooks, ShippedFileReadsWithoutWarnings) {
    std::vector<std::string> warnings;
    const looks::BossLooks shipped =
        looks::readBossLooks(config::ConfigFile::load(std::string(RTYPE_CLIENT_CONFIG_DIR) + "/bosses.toml"), warnings);
    EXPECT_TRUE(warnings.empty()) << (warnings.empty() ? "" : warnings.front());
    EXPECT_FALSE(shipped.bosses.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
