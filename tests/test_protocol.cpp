#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
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

// Action ids are opaque to the protocol: any 32-bit value (in a game, input::makeActionId("shoot")...).
constexpr ActionId SHOOT = 0xDEADBEEF;
constexpr ActionId MOVE_X = 0x00000001;
constexpr ActionId MOVE_Y = 0xFFFFFFFF;

constexpr float NOT_A_NUMBER = std::numeric_limits<float>::quiet_NaN();
constexpr float INFINITE = std::numeric_limits<float>::infinity();

PlayerInput inputWithAxis(const float value) {
    return PlayerInput{.sequence = 1, .held = {}, .axes = {{.action = MOVE_X, .value = value}}};
}

WorldState snapshotAt(const float x, const float y) {
    return WorldState{.tick = 1, .lastProcessedInput = 0, .entities = {{.entityId = 1, .x = x, .y = y}}};
}

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
        LevelStart{.levelIndex = 2, .levelCount = 3, .name = "Asteroid Belt"},
        LevelStart{.levelIndex = 0, .levelCount = 1, .name = ""},
        LevelComplete{.levelIndex = 1, .score = 42, .playerScores = {20, 22}},
        Leave{},
        Chat{.clientId = 2, .text = "hello \xC3\xA9"},
        PlayerInput{.sequence = 0xFFFFFFFF,
                    .held = {SHOOT, MOVE_Y},
                    .axes = {{.action = MOVE_X, .value = -1.0F}, {.action = MOVE_Y, .value = 0.25F}}},
        PlayerInput{.sequence = 3, .held = {}, .axes = {}},
        WorldState{
            .tick = 42,
            .lastProcessedInput = 41,
            .entities =
                {{.entityId = 1,
                  .kind = makeEntityKind("player"),
                  .subtype = 0,
                  .x = -12345.678F,
                  .y = 1.0e30F,
                  .health = 255,
                  .flags = 0x01},
                 {.entityId = 99, .kind = 0xFFFFFFFF, .subtype = 3, .x = 0.5F, .y = -0.0F, .health = 0, .flags = 0}}},
    };
    for (const auto& message : messages) {
        EXPECT_EQ(roundTrip(message), message) << "variant index " << message.index();
    }
}

TEST(Protocol, InputIsCompact) {
    // Sent every tick over UDP. Idle player: envelope tag + length + sequence.
    EXPECT_LE(encode(PlayerInput{.sequence = 100, .held = {}, .axes = {}}).size(), 4U);
    // R-Type worst case: shooting while moving diagonally (1 button, 2 axes) = 4 + 6 + 2 * 12 bytes.
    const PlayerInput busy{.sequence = 100,
                           .held = {SHOOT},
                           .axes = {{.action = MOVE_X, .value = 1.0F}, {.action = MOVE_Y, .value = -1.0F}}};
    EXPECT_LE(encode(busy).size(), 34U);
}

TEST(Protocol, FullSnapshotFitsInADatagram) {
    WorldState snapshot{.tick = 0xFFFFFFFF, .lastProcessedInput = 0xFFFFFFFF, .entities = {}};
    snapshot.partIndex = MAX_SNAPSHOT_PARTS - 1;
    snapshot.partCount = MAX_SNAPSHOT_PARTS;
    // Every field at its largest encoding: 19 bytes of header + 32 per entity.
    for (std::uint32_t i = 0; i < MAX_ENTITIES_PER_PART; ++i) {
        snapshot.entities.push_back({.entityId = 0xFFFFFFF0 + i,
                                     .kind = 0xFFFFFFFF,
                                     .subtype = 255,
                                     .x = -32768.0F,
                                     .y = -32768.0F,
                                     .health = 255,
                                     .flags = 255});
    }
    EXPECT_LE(encode(snapshot).size(), MAX_UNRELIABLE_SIZE);
}

TEST(Protocol, SnapshotPartSurvivesARoundTrip) {
    const WorldState part{.tick = 3, .lastProcessedInput = 2, .entities = {}, .partIndex = 4, .partCount = 9};

    const auto decoded = decode(encode(part));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(std::get<WorldState>(*decoded), part);
}

TEST(Protocol, UnsplitSnapshotHasOnePart) {
    const auto decoded = decode(encode(WorldState{.tick = 3, .lastProcessedInput = 2, .entities = {}}));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(std::get<WorldState>(*decoded).partIndex, 0U);
    EXPECT_EQ(std::get<WorldState>(*decoded).partCount, 1U);
}

TEST(Protocol, RejectsInvalidSnapshotParts) {
    const auto withPart = [](const std::uint32_t index, const std::uint32_t count) {
        return encode(
            WorldState{.tick = 1, .lastProcessedInput = 0, .entities = {}, .partIndex = index, .partCount = count});
    };

    EXPECT_FALSE(decode(withPart(2, 2)).has_value()) << "index past the last part";
    EXPECT_FALSE(decode(withPart(0, MAX_SNAPSHOT_PARTS + 1)).has_value()) << "too many parts";
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
    EXPECT_FALSE(decode(encode(snapshotAt(NOT_A_NUMBER, 0.0F))).has_value());
    EXPECT_FALSE(decode(encode(snapshotAt(0.0F, INFINITE))).has_value());
    EXPECT_FALSE(decode(encode(snapshotAt(-INFINITE, 0.0F))).has_value());
    EXPECT_TRUE(decode(encode(snapshotAt(std::numeric_limits<float>::max(), -1.0e-30F))).has_value());

    // WorldState { entities { x: NaN } } by hand: x is field 8, fixed32 (wire type 5), NaN = 0x7FC00000.
    const std::vector<std::uint8_t> nanX = {TAG_WORLD_STATE, 0x07, 0x1A, 0x05, 0x45, 0x00, 0x00, 0xC0, 0x7F};
    EXPECT_FALSE(decode(nanX).has_value());
}

TEST(Protocol, IgnoresVersion1EntityType) {
    // WorldState { entities { type (field 2, reserved since v2): 9 } }: the unknown field is skipped, the kind stays 0.
    const std::vector<std::uint8_t> bytes = {TAG_WORLD_STATE, 0x04, 0x1A, 0x02, 0x10, 0x09};
    const auto decoded = decode(bytes);
    ASSERT_TRUE(decoded.has_value());
    const auto* snapshot = std::get_if<WorldState>(&*decoded);
    ASSERT_NE(snapshot, nullptr);
    ASSERT_EQ(snapshot->entities.size(), 1U);
    EXPECT_EQ(snapshot->entities.front().kind, 0U);
}

TEST(Protocol, EntityKindIsTheFnv1aHashOfThePrefabName) {
    // Reference FNV-1a 32-bit values: the client and the server of any language must agree on them.
    static_assert(makeEntityKind("") == 0x811C9DC5);
    EXPECT_EQ(makeEntityKind("a"), 0xE40C292CU);
    EXPECT_EQ(makeEntityKind("foobar"), 0xBF9CF968U);
    EXPECT_NE(makeEntityKind("missile"), makeEntityKind("player_bullet"));
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

TEST(Protocol, RejectsInvalidInputs) {
    PlayerInput tooManyButtons{.sequence = 1, .held = {}, .axes = {}};
    tooManyButtons.held.resize(MAX_INPUT_BUTTONS + 1, SHOOT);
    EXPECT_FALSE(decode(encode(tooManyButtons)).has_value());
    tooManyButtons.held.pop_back();
    EXPECT_TRUE(decode(encode(tooManyButtons)).has_value());

    PlayerInput tooManyAxes{.sequence = 1, .held = {}, .axes = {}};
    tooManyAxes.axes.resize(MAX_INPUT_AXES + 1, {.action = MOVE_X, .value = 0.5F});
    EXPECT_FALSE(decode(encode(tooManyAxes)).has_value());
    tooManyAxes.axes.pop_back();
    EXPECT_TRUE(decode(encode(tooManyAxes)).has_value());

    EXPECT_FALSE(decode(encode(inputWithAxis(1.5F))).has_value());
    EXPECT_FALSE(decode(encode(inputWithAxis(-1.01F))).has_value());
    EXPECT_FALSE(decode(encode(inputWithAxis(NOT_A_NUMBER))).has_value());
    EXPECT_FALSE(decode(encode(inputWithAxis(INFINITE))).has_value());
    EXPECT_TRUE(decode(encode(inputWithAxis(1.0F))).has_value());
    EXPECT_TRUE(decode(encode(inputWithAxis(-1.0F))).has_value());
}

TEST(Protocol, IgnoresVersion1InputBitmask) {
    // PlayerInput { sequence: 1, buttons (field 2, reserved since v2): 0xFF }: the unknown field is skipped, never
    // misread as an action.
    const std::vector<std::uint8_t> bytes = {TAG_PLAYER_INPUT, 0x05, 0x08, 0x01, 0x10, 0xFF, 0x01};
    const auto decoded = decode(bytes);
    ASSERT_TRUE(decoded.has_value());
    const Message expected = PlayerInput{.sequence = 1, .held = {}, .axes = {}};
    EXPECT_EQ(*decoded, expected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(Protocol, RejectsInvalidLevelMessages) {
    EXPECT_FALSE(decode(encode(LevelStart{.levelIndex = 0, .levelCount = 0, .name = "x"})).has_value());
    EXPECT_FALSE(decode(encode(LevelStart{.levelIndex = 3, .levelCount = 3, .name = "x"})).has_value());
    EXPECT_FALSE(decode(encode(LevelStart{
                            .levelIndex = 0, .levelCount = static_cast<std::uint8_t>(MAX_LEVELS + 1), .name = "x"}))
                     .has_value());
    EXPECT_FALSE(decode(encode(LevelStart{
                            .levelIndex = 0, .levelCount = 1, .name = std::string(MAX_LEVEL_NAME_LENGTH + 1, 'a')}))
                     .has_value());
    EXPECT_TRUE(
        decode(encode(LevelStart{.levelIndex = 0, .levelCount = 1, .name = std::string(MAX_LEVEL_NAME_LENGTH, 'a')}))
            .has_value());

    EXPECT_FALSE(decode(encode(LevelComplete{.levelIndex = static_cast<std::uint8_t>(MAX_LEVELS)})).has_value());
    LevelComplete complete{.levelIndex = 0, .score = 0, .playerScores = {}};
    complete.playerScores.resize(MAX_PLAYERS + 1);
    EXPECT_FALSE(decode(encode(complete)).has_value());
}
