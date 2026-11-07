#include <gtest/gtest.h>

#include <cstdint>
#include <variant>
#include <vector>

#include "CommandLine.hpp"
#include "protocol/Protocol.hpp"

namespace protocol = rlss::protocol;

TEST(RlssProtocol, LobbyStateRoundTrips) {
    const auto bytes = protocol::encode(protocol::LobbyState{.players = 300});
    EXPECT_EQ(bytes, (std::vector<std::uint8_t>{1, 0x01, 0x2C}));

    const auto message = protocol::decode(bytes);
    ASSERT_TRUE(message.has_value());
    ASSERT_TRUE(std::holds_alternative<protocol::LobbyState>(*message));
    EXPECT_EQ(std::get<protocol::LobbyState>(*message).players, 300);
}

TEST(RlssProtocol, RejectsMalformedMessages) {
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{}).has_value());
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{1, 0}).has_value());
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{1, 0, 2, 3}).has_value());
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{0xFF, 0, 2}).has_value());
}

TEST(RlssProtocol, SnapshotRoundTrips) {
    const protocol::Snapshot snapshot{
        .tick = 77,
        .lastProcessedInput = 12,
        .partIndex = 1,
        .partCount = 2,
        .entities = {{.id = 5,
                      .kind = 1,
                      .x = 10.5F,
                      .y = -3.25F,
                      .rotation = 1.5F,
                      .facingLeft = true,
                      .color = 3,
                      .boost = 64,
                      .boosting = true},
                     {.id = 9, .kind = 1, .x = 0.0F, .y = 1000.0F, .rotation = -0.5F, .facingLeft = false, .color = 0}},
    };
    const auto bytes = protocol::encode(snapshot);
    EXPECT_EQ(bytes.size(), 12U + (2U * 20U));

    const auto message = protocol::decode(bytes);
    ASSERT_TRUE(message.has_value());
    const auto& decoded = std::get<protocol::Snapshot>(*message);
    EXPECT_EQ(decoded.tick, 77U);
    EXPECT_EQ(decoded.lastProcessedInput, 12U);
    EXPECT_EQ(decoded.partIndex, 1);
    EXPECT_EQ(decoded.partCount, 2);
    ASSERT_EQ(decoded.entities.size(), 2U);
    EXPECT_EQ(decoded.entities[0].id, 5U);
    EXPECT_FLOAT_EQ(decoded.entities[0].x, 10.5F);
    EXPECT_FLOAT_EQ(decoded.entities[0].y, -3.25F);
    EXPECT_FLOAT_EQ(decoded.entities[0].rotation, 1.5F);
    EXPECT_TRUE(decoded.entities[0].facingLeft);
    EXPECT_EQ(decoded.entities[0].color, 3);
    EXPECT_EQ(decoded.entities[0].boost, 64);
    EXPECT_TRUE(decoded.entities[0].boosting);
    EXPECT_FALSE(decoded.entities[1].boosting);
    EXPECT_FALSE(decoded.entities[1].facingLeft);
}

TEST(RlssProtocol, RejectsMalformedSnapshots) {
    const protocol::Snapshot valid{.tick = 1,
                                   .lastProcessedInput = 0,
                                   .partIndex = 0,
                                   .partCount = 1,
                                   .entities = {{.id = 1, .kind = 1, .x = 0.0F, .y = 0.0F, .rotation = 0.0F}}};
    const auto bytes = protocol::encode(valid);
    ASSERT_TRUE(protocol::decode(bytes).has_value());

    auto truncated = bytes;
    truncated.pop_back();
    EXPECT_FALSE(protocol::decode(truncated).has_value());

    auto badPart = bytes;
    badPart[9] = 1;  // partIndex == partCount
    EXPECT_FALSE(protocol::decode(badPart).has_value());

    auto nan = bytes;
    nan[17] = 0x7F;  // x = NaN
    nan[18] = 0xC0;
    EXPECT_FALSE(protocol::decode(nan).has_value());

    auto zeroId = bytes;
    zeroId[15] = 0;
    EXPECT_FALSE(protocol::decode(zeroId).has_value());

    auto tooMuchBoost = bytes;
    tooMuchBoost.back() = 101;
    EXPECT_FALSE(protocol::decode(tooMuchBoost).has_value());

    auto tooMany = bytes;
    tooMany[11] = static_cast<std::uint8_t>(protocol::MAX_ENTITIES_PER_PART + 1);
    EXPECT_FALSE(protocol::decode(tooMany).has_value());
}

TEST(RlssProtocol, PlayerInputRoundTrips) {
    const auto bytes =
        protocol::encode(protocol::PlayerInput{.sequence = 300, .steer = -1.0F, .jump = true, .boost = true});
    EXPECT_EQ(bytes.size(), 7U);
    const auto message = protocol::decode(bytes);
    ASSERT_TRUE(message.has_value());
    const auto& input = std::get<protocol::PlayerInput>(*message);
    EXPECT_EQ(input.sequence, 300U);
    EXPECT_FLOAT_EQ(input.steer, -1.0F);
    EXPECT_TRUE(input.jump);
    EXPECT_TRUE(input.boost);

    // Out of range or non-finite steering is clamped when encoding.
    const auto clamped = protocol::decode(protocol::encode(protocol::PlayerInput{.sequence = 1, .steer = 7.0F}));
    EXPECT_FLOAT_EQ(std::get<protocol::PlayerInput>(*clamped).steer, 1.0F);
}

TEST(RlssProtocol, RejectsMalformedInputs) {
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{3, 0, 0, 0, 1, 0}).has_value());        // too short
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{3, 0, 0, 0, 1, 0, 0, 0}).has_value());  // too long
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{3, 0, 0, 0, 1, 0x80, 0}).has_value());  // steer -128
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{3, 0, 0, 0, 1, 0, 0x04}).has_value());  // unknown button
}

TEST(RlssProtocol, MatchStatusRoundTrips) {
    const protocol::MatchStatus status{
        .blueScore = 3, .orangeScore = 12, .phase = protocol::MatchPhase::Kickoff, .countdown = 2};
    const auto bytes = protocol::encode(status);
    EXPECT_EQ(bytes, (std::vector<std::uint8_t>{4, 0, 3, 0, 12, 2, 2}));
    const auto message = protocol::decode(bytes);
    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(std::get<protocol::MatchStatus>(*message), status);
}

TEST(RlssProtocol, RejectsMalformedMatchStatus) {
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{4, 0, 3, 0, 12, 2}).has_value());        // too short
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{4, 0, 3, 0, 12, 3, 0}).has_value());     // unknown phase
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{4, 0, 3, 0, 12, 2, 10}).has_value());    // countdown > 9
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{4, 0, 3, 0, 12, 2, 2, 0}).has_value());  // too long
}

TEST(RlssProtocol, WelcomeRoundTrips) {
    const auto bytes = protocol::encode(protocol::Welcome{.carId = 258});
    EXPECT_EQ(bytes, (std::vector<std::uint8_t>{5, 0, 0, 1, 2}));
    const auto message = protocol::decode(bytes);
    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(std::get<protocol::Welcome>(*message).carId, 258U);
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{5, 0, 0, 0, 0}).has_value());     // no car 0
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{5, 0, 0, 1}).has_value());        // too short
    EXPECT_FALSE(protocol::decode(std::vector<std::uint8_t>{5, 0, 0, 1, 2, 3}).has_value());  // too long
}

TEST(RlssCommandLine, ParsesPorts) {
    EXPECT_EQ(rlss::parsePort("4250"), 4250);
    EXPECT_FALSE(rlss::parsePort("0").has_value());
    EXPECT_FALSE(rlss::parsePort("65536").has_value());
    EXPECT_FALSE(rlss::parsePort("12ab").has_value());
    EXPECT_FALSE(rlss::parsePort("").has_value());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
