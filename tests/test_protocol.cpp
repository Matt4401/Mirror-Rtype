#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

using namespace rtype::protocol;

namespace {

// Hand-written protobuf bytes, to test what a malicious or buggy peer could send.
// Envelope field numbers: player_input = 1, world_state = 2 (see rtype.proto).
constexpr std::uint8_t TAG_PLAYER_INPUT = (1 << 3) | 2;
constexpr std::uint8_t TAG_WORLD_STATE = (2 << 3) | 2;

Message roundTrip(const Message& message) {
    const auto bytes = encode(message);
    EXPECT_FALSE(bytes.empty());
    const auto decoded = decode(bytes);
    EXPECT_TRUE(decoded.has_value());
    return decoded.value_or(Message{});
}

}  // namespace

TEST(Protocol, RoundTripsEveryMessage) {
    const std::vector<Message> messages = {
        Hello{.version = PROTOCOL_VERSION},
        Welcome{.udpPort = 4242},
        LobbyListRequest{},
        LobbyList{.rooms = {{.roomId = 1, .playerCount = 2, .maxPlayers = 4, .state = RoomState::Waiting},
                            {.roomId = 7, .playerCount = 4, .maxPlayers = 4, .state = RoomState::InGame}}},
        LobbyCreate{},
        LobbyJoin{.roomId = 12},
        LobbyReady{.ready = true},
        LobbyUpdate{.roomId = 3, .slots = {{.clientId = 1, .ready = true}, {.clientId = 9, .ready = false}}},
        GameStart{.roomId = 3, .playerIndex = 1, .playerCount = 2},
        GameOver{.reason = GameOverReason::Victory, .score = 123456, .playerScores = {100, 200, 0, 0}},
        PlayerLeft{.clientId = 5},
        Leave{},
        Chat{.clientId = 2, .text = "hello \xC3\xA9"},
        PlayerInput{.sequence = 0xFFFFFFFF, .buttons = BUTTON_UP | BUTTON_SHOOT},
        WorldState{.tick = 42,
                   .lastProcessedInput = 41,
                   .entities = {{.entityId = 1,
                                 .type = EntityType::Player,
                                 .subtype = 0,
                                 .x = -32768,
                                 .y = 32767,
                                 .health = 255,
                                 .flags = ENTITY_ALIVE},
                                {.entityId = 99,
                                 .type = EntityType::Powerup,
                                 .subtype = 3,
                                 .x = 10,
                                 .y = -10,
                                 .health = 0,
                                 .flags = 0}}},
    };
    for (const auto& message : messages) {
        EXPECT_EQ(roundTrip(message), message) << "variant index " << message.index();
    }
}

TEST(Protocol, InputIsCompact) {
    // Sent every tick over UDP: envelope tag + length + 2 small fields.
    EXPECT_LE(encode(PlayerInput{.sequence = 100, .buttons = BUTTON_RIGHT}).size(), 8U);
}

TEST(Protocol, FullSnapshotFitsInADatagram) {
    WorldState snapshot{.tick = 0xFFFFFFFF, .lastProcessedInput = 0xFFFFFFFF, .entities = {}};
    for (std::uint32_t i = 0; i < 40; ++i) {
        snapshot.entities.push_back({.entityId = 0xFFFFFFF0 + i,
                                     .type = EntityType::EnemyMissile,
                                     .subtype = 255,
                                     .x = -32768,
                                     .y = -32768,
                                     .health = 255,
                                     .flags = 255});
    }
    EXPECT_LE(encode(snapshot).size(), MAX_UNRELIABLE_SIZE);
}

TEST(Protocol, RejectsEmptyAndOversizedPayloads) {
    EXPECT_FALSE(decode({}).has_value());
    const std::vector<std::uint8_t> huge(MAX_MESSAGE_SIZE + 1, 0);
    EXPECT_FALSE(decode(huge).has_value());
}

TEST(Protocol, RejectsGarbage) {
    const std::vector<std::vector<std::uint8_t>> payloads = {
        {0xFF},                                // truncated tag
        {TAG_PLAYER_INPUT, 0x05, 0x08},        // length longer than the payload
        {0x52, 0x54, 0x01, 0x60},              // old hand-made header
        {TAG_PLAYER_INPUT, 0x80, 0x80, 0x80},  // truncated varint length
        {0x07},                                // invalid wire type
    };
    for (const auto& payload : payloads) {
        EXPECT_FALSE(decode(payload).has_value());
    }
}

TEST(Protocol, RejectsEnvelopeWithoutKnownMessage) {
    // An unknown field (number 1000, varint): parsed by protobuf, but no message case is set.
    const std::vector<std::uint8_t> unknown = {0xC0, 0x3E, 0x01};
    EXPECT_FALSE(decode(unknown).has_value());
}

TEST(Protocol, RejectsOutOfRangeEntity) {
    // WorldState { entities { x: 40000 } }, x is sint32 (zigzag 80000 = 0x80 0xF1 0x04).
    const std::vector<std::uint8_t> badX = {TAG_WORLD_STATE, 0x06, 0x1A, 0x04, 0x20, 0x80, 0xF1, 0x04};
    EXPECT_FALSE(decode(badX).has_value());

    // WorldState { entities { type: 9 } }: unknown enum value.
    const std::vector<std::uint8_t> badType = {TAG_WORLD_STATE, 0x04, 0x1A, 0x02, 0x10, 0x09};
    EXPECT_FALSE(decode(badType).has_value());
}

TEST(Protocol, RejectsOutOfRangeLists) {
    LobbyUpdate update{.roomId = 1, .slots = {}};
    update.slots.resize(MAX_PLAYERS + 1);
    EXPECT_FALSE(decode(encode(update)).has_value());

    GameOver over{.reason = GameOverReason::AllDead, .score = 0, .playerScores = {}};
    over.playerScores.resize(MAX_PLAYERS + 1);
    EXPECT_FALSE(decode(encode(over)).has_value());
}

TEST(Protocol, RejectsInvalidRoomEntries) {
    const auto list = [](const std::uint8_t playerCount, const std::uint8_t maxPlayers) {
        return LobbyList{.rooms = {{.roomId = 1, .playerCount = playerCount, .maxPlayers = maxPlayers}}};
    };
    EXPECT_FALSE(decode(encode(list(0, 0))).has_value());
    EXPECT_FALSE(decode(encode(list(4, 1))).has_value());
    EXPECT_FALSE(decode(encode(list(5, 5))).has_value());
    EXPECT_TRUE(decode(encode(list(0, 1))).has_value());
    EXPECT_TRUE(decode(encode(list(4, 4))).has_value());
}

TEST(Protocol, RejectsInvalidFields) {
    EXPECT_FALSE(decode(encode(Welcome{.udpPort = 0})).has_value());
    EXPECT_FALSE(decode(encode(GameStart{.roomId = 1, .playerIndex = 2, .playerCount = 2})).has_value());
    EXPECT_FALSE(decode(encode(Chat{.clientId = 1, .text = ""})).has_value());
    EXPECT_FALSE(decode(encode(Chat{.clientId = 1, .text = std::string(MAX_CHAT_LENGTH + 1, 'a')})).has_value());
    EXPECT_TRUE(decode(encode(Chat{.clientId = 1, .text = std::string(MAX_CHAT_LENGTH, 'a')})).has_value());
}

TEST(Protocol, ClearsReservedButtonBits) {
    // PlayerInput { sequence: 1, buttons: 0xFF }
    const std::vector<std::uint8_t> bytes = {TAG_PLAYER_INPUT, 0x05, 0x08, 0x01, 0x10, 0xFF, 0x01};
    const auto decoded = decode(bytes);
    ASSERT_TRUE(decoded.has_value());
    const auto* input = std::get_if<PlayerInput>(&*decoded);
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->sequence, 1U);
    EXPECT_EQ(input->buttons, BUTTONS_MASK);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
