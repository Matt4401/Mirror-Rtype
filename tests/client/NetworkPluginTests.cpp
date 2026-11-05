#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "components/Interpolated.hpp"
#include "components/Predicted.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "net/ClientConnection.hpp"
#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/ReplicationPlugins.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
using ecs::net::NetworkEvent;
namespace protocol = rtype::protocol;
namespace game = rtype::game;

constexpr float DT = 1.0F / 60.0F;
constexpr std::uint16_t UDP_PORT = 4242;
constexpr std::uint32_t ROOM = 7;
constexpr std::uint32_t MY_SHIP = 10;
constexpr std::uint32_t OTHER_SHIP = 11;

/** @brief Client transport the test talks through, in place of the server. */
class FakeClient final : public ecs::net::INetworkClient {
  public:
    struct Sent {
        Bytes data;
        Channel channel;
    };

    bool connect(const std::string& /*host*/, std::uint16_t /*port*/) override { return true; }
    bool openUdp(const std::uint16_t port) override {
        udpPort = port;
        return true;
    }
    void closeUdp() override {}
    void send(const Bytes& data, const Channel channel) override { sent.push_back({.data = data, .channel = channel}); }
    std::vector<NetworkEvent> poll() override { return std::exchange(next, {}); }
    void stop() override {}
    [[nodiscard]] bool isRunning() const override { return true; }

    std::vector<Sent> sent;
    std::vector<NetworkEvent> next;
    std::optional<std::uint16_t> udpPort;
};

class RTypeNetworkPluginTest : public ::testing::Test {
  protected:
    RTypeNetworkPluginTest() {
        const auto connection = std::make_shared<engine::ClientConnection>(transport);
        engine.addPlugin<ecs::net::NetworkPlugin>(connection, connection);
        engine.addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<game::ReplicationCodec>());
        engine.addPlugin<client::RTypeNetworkPlugin>(transport);
        engine.init();
        inputEntity = engine.registry().spawnEntity();
        engine.registry().addComponent(inputEntity, ::input::InputState{.playerIndex = 0, .actions = {}});
    }

    /** @brief The server sends a message; the client handles it during the frame this runs. */
    void receive(const protocol::Message& message, const Channel channel = Channel::Reliable) {
        transport->next.push_back({.type = EventType::Data,
                                   .peer = ecs::net::SERVER_PEER,
                                   .data = protocol::encode(message),
                                   .channel = channel});
        engine.step(DT);
    }

    /** @brief Messages of one type the client sent, oldest first. */
    template <typename Message>
    [[nodiscard]] std::vector<Message> sentMessages() const {
        std::vector<Message> messages;
        for (const auto& sent : transport->sent) {
            const auto decoded = protocol::decode(sent.data);
            if (decoded && std::holds_alternative<Message>(*decoded)) {
                messages.push_back(std::get<Message>(*decoded));
            }
        }
        return messages;
    }

    void pressReady() {
        actions()[client::input::ACTION_READY].justPressed = true;
        engine.step(DT);
        actions()[client::input::ACTION_READY].justPressed = false;
    }

    std::unordered_map<::input::ActionId, ::input::ActionData>& actions() {
        return engine.registry().getComponents<::input::InputState>().get(inputEntity).actions;
    }

    /** @brief Snapshot with the ship of player 0 and the ship of player 1. */
    static protocol::WorldState twoShips(const std::uint32_t tick, const float myX = 100.0F) {
        return protocol::WorldState{
            .tick = tick,
            .lastProcessedInput = 0,
            .entities = {
                {.entityId = MY_SHIP, .kind = game::KIND_PLAYER_SHIP, .subtype = 0, .x = myX, .y = 100.0F},
                {.entityId = OTHER_SHIP, .kind = game::KIND_PLAYER_SHIP, .subtype = 1, .x = 200.0F, .y = 100.0F}}};
    }

    [[nodiscard]] ecs::Entity entityOf(const std::uint32_t id) {
        return engine.registry().getResource<ecs::net::NetworkEntityMap>().entityOf(id).value();
    }

    std::shared_ptr<FakeClient> transport = std::make_shared<FakeClient>();
    engine::Engine engine;
    ecs::Entity inputEntity = 0;
};

}  // namespace

// --- Session ---------------------------------------------------------------------------------------------------------

TEST_F(RTypeNetworkPluginTest, SaysHelloOnceConnected) {
    transport->next.push_back({.type = EventType::Connected, .peer = 0, .data = {}, .channel = Channel::Reliable});
    engine.step(DT);

    ASSERT_EQ(sentMessages<protocol::Hello>().size(), 1U);
    EXPECT_EQ(sentMessages<protocol::Hello>()[0].version, protocol::PROTOCOL_VERSION);
    EXPECT_EQ(transport->sent[0].channel, Channel::Reliable);
}

TEST_F(RTypeNetworkPluginTest, WelcomeAsksForTheRoomsAndUdpWaitsForTheGameStart) {
    receive(protocol::Welcome{.udpPort = UDP_PORT});

    EXPECT_FALSE(transport->udpPort.has_value()) << "the RFC opens UDP at GameStart";
    EXPECT_EQ(sentMessages<protocol::LobbyListRequest>().size(), 1U);

    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 1});
    EXPECT_EQ(transport->udpPort, UDP_PORT);
}

TEST_F(RTypeNetworkPluginTest, IgnoresMalformedAndUnreliableMessages) {
    transport->next.push_back({.type = EventType::Data, .peer = 0, .data = {0xFF, 0x13}, .channel = Channel::Reliable});
    receive(protocol::Welcome{.udpPort = UDP_PORT}, Channel::Unreliable);

    EXPECT_TRUE(sentMessages<protocol::LobbyListRequest>().empty()) << "Welcome is only valid on the reliable channel";
}

TEST_F(RTypeNetworkPluginTest, StopsTheEngineWhenTheServerIsLost) {
    transport->next.push_back({.type = EventType::Disconnected, .peer = 0, .data = {}, .channel = Channel::Reliable});

    engine.run();  // returns only because the plugin stops the loop

    EXPECT_FALSE(engine.isRunning());
}

TEST_F(RTypeNetworkPluginTest, LeavesOnShutdown) {
    engine.shutdown();

    EXPECT_EQ(sentMessages<protocol::Leave>().size(), 1U);
}

// --- Lobby -----------------------------------------------------------------------------------------------------------

TEST_F(RTypeNetworkPluginTest, CreatesARoomWhenNoneIsOpen) {
    receive(protocol::LobbyList{
        .rooms = {{.roomId = 1, .playerCount = 4, .maxPlayers = 4, .state = protocol::RoomState::Waiting},
                  {.roomId = 2, .playerCount = 1, .maxPlayers = 4, .state = protocol::RoomState::InGame}}});

    EXPECT_EQ(sentMessages<protocol::LobbyCreate>().size(), 1U);
    EXPECT_TRUE(sentMessages<protocol::LobbyJoin>().empty());
}

TEST_F(RTypeNetworkPluginTest, JoinsTheFirstOpenRoom) {
    receive(protocol::LobbyList{
        .rooms = {{.roomId = 2, .playerCount = 1, .maxPlayers = 4, .state = protocol::RoomState::InGame},
                  {.roomId = ROOM, .playerCount = 1, .maxPlayers = 4, .state = protocol::RoomState::Waiting}}});

    ASSERT_EQ(sentMessages<protocol::LobbyJoin>().size(), 1U);
    EXPECT_EQ(sentMessages<protocol::LobbyJoin>()[0].roomId, ROOM);
    EXPECT_TRUE(sentMessages<protocol::LobbyCreate>().empty());
}

TEST_F(RTypeNetworkPluginTest, TriesAgainWhenTheJoinedRoomStartedMeanwhile) {
    receive(protocol::LobbyList{
        .rooms = {{.roomId = ROOM, .playerCount = 1, .maxPlayers = 4, .state = protocol::RoomState::Waiting}}});
    // The join was refused: the server only pushes the list after the room changed.
    receive(protocol::LobbyList{
        .rooms = {{.roomId = ROOM, .playerCount = 1, .maxPlayers = 4, .state = protocol::RoomState::InGame}}});

    EXPECT_EQ(sentMessages<protocol::LobbyCreate>().size(), 1U);
}

TEST_F(RTypeNetworkPluginTest, StaysInItsRoomWhenTheListChanges) {
    receive(protocol::LobbyUpdate{.roomId = ROOM, .slots = {{.clientId = 1, .ready = false}}});
    receive(protocol::LobbyList{.rooms = {}});

    EXPECT_TRUE(sentMessages<protocol::LobbyCreate>().empty());
    EXPECT_TRUE(sentMessages<protocol::LobbyJoin>().empty());
}

TEST_F(RTypeNetworkPluginTest, ReadyKeyTogglesTheReadyState) {
    receive(protocol::LobbyUpdate{.roomId = ROOM, .slots = {{.clientId = 1, .ready = false}}});

    pressReady();
    pressReady();

    const auto sent = sentMessages<protocol::LobbyReady>();
    ASSERT_EQ(sent.size(), 2U);
    EXPECT_TRUE(sent[0].ready);
    EXPECT_FALSE(sent[1].ready);
}

TEST_F(RTypeNetworkPluginTest, ReadyKeyDoesNothingOutsideARoomOrInGame) {
    pressReady();
    EXPECT_TRUE(sentMessages<protocol::LobbyReady>().empty());

    receive(protocol::LobbyUpdate{.roomId = ROOM, .slots = {{.clientId = 1, .ready = false}}});
    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 1});
    pressReady();
    EXPECT_TRUE(sentMessages<protocol::LobbyReady>().empty());
}

// --- In game ---------------------------------------------------------------------------------------------------------

TEST_F(RTypeNetworkPluginTest, SendsNoInputBeforeTheGameStarts) {
    engine.step(DT);

    EXPECT_TRUE(sentMessages<protocol::PlayerInput>().empty());
}

TEST_F(RTypeNetworkPluginTest, SendsTheActionsOfEachStepOverUdp) {
    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 1});
    transport->sent.clear();
    actions()[game::ACTION_MOVE_X].axis = -1.0F;
    actions()[::input::makeActionId("shoot")].held = true;
    actions()[client::input::ACTION_READY].held = true;
    actions()[::input::makeActionId("charge_weapon")].held = false;

    engine.step(DT);
    engine.step(DT);

    const auto inputs = sentMessages<protocol::PlayerInput>();
    ASSERT_EQ(inputs.size(), 2U);
    EXPECT_EQ(inputs[1].sequence, inputs[0].sequence + 1);
    EXPECT_EQ(inputs[0].held, std::vector<protocol::ActionId>{::input::makeActionId("shoot")})
        << "ready is not a game input";
    ASSERT_EQ(inputs[0].axes.size(), 1U);
    EXPECT_EQ(inputs[0].axes[0].action, game::ACTION_MOVE_X);
    EXPECT_FLOAT_EQ(inputs[0].axes[0].value, -1.0F);
    EXPECT_EQ(transport->sent[0].channel, Channel::Unreliable);
}

TEST_F(RTypeNetworkPluginTest, LocalShipIsPredictedAndTheOthersInterpolated) {
    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 2});
    receive(twoShips(1), Channel::Unreliable);

    auto& registry = engine.registry();
    EXPECT_TRUE(registry.hasComponent<ecs::Predicted>(entityOf(MY_SHIP)));
    EXPECT_FALSE(registry.hasComponent<ecs::Predicted>(entityOf(OTHER_SHIP)));
    EXPECT_TRUE(registry.hasComponent<ecs::Interpolated>(entityOf(OTHER_SHIP)));
}

TEST_F(RTypeNetworkPluginTest, LocalShipMovesAtOnceAndOnlyIt) {
    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 2});
    receive(twoShips(1), Channel::Unreliable);
    actions()[game::ACTION_MOVE_X].axis = 1.0F;
    const auto& transforms = engine.registry().getComponents<ecs::Transform>();
    const float mine = transforms.get(entityOf(MY_SHIP)).x;
    const float other = transforms.get(entityOf(OTHER_SHIP)).x;

    engine.step(DT);

    EXPECT_FLOAT_EQ(transforms.get(entityOf(MY_SHIP)).x, mine + (game::SHIP_SPEED * DT));
    EXPECT_FLOAT_EQ(transforms.get(entityOf(OTHER_SHIP)).x, other);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
