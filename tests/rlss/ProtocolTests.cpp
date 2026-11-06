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
