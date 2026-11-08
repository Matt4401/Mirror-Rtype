#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ClientOptions.hpp"
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
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PlayerControlPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/SessionEvents.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
using ecs::net::NetworkEvent;
namespace protocol = rtype::protocol;
namespace game = rtype::game;

// A power of two: exact in binary, so that adding it to the fake clock makes exactly one step due.
constexpr float DT = 1.0F / 64.0F;
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

    bool connect(const std::string& host, std::uint16_t port) override {
        connectedTo = host + ":" + std::to_string(port);
        return connects;
    }
    [[nodiscard]] ecs::net::DisconnectReason disconnectReason() const override { return reason; }
    bool openUdp(const std::uint16_t port) override {
        udpPort = port;
        return udpOpens;
    }
    void closeUdp() override { ++udpClosed; }
    void send(const Bytes& data, const Channel channel) override { sent.push_back({.data = data, .channel = channel}); }
    std::vector<NetworkEvent> poll() override { return std::exchange(next, {}); }
    void stop() override { stopped = true; }
    [[nodiscard]] bool isRunning() const override { return true; }

    std::vector<Sent> sent;
    std::vector<NetworkEvent> next;
    std::optional<std::uint16_t> udpPort;
    bool udpOpens = true;
    bool connects = true;
    bool stopped = false;
    int udpClosed = 0;
    std::string connectedTo;
    ecs::net::DisconnectReason reason = ecs::net::DisconnectReason::ConnectionLost;
};

/** @brief Client engine with the network plugins; every connection uses the next fake transport. */
class RTypeNetworkPluginTest : public ::testing::Test {
  protected:
    RTypeNetworkPluginTest() {
        engine.setFixedDeltaTime(DT);
        const auto connection = std::make_shared<ecs::net::ClientConnection>(nullptr);
        engine.addPlugin<ecs::net::NetworkPlugin>(connection, connection);
        engine.addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<game::ReplicationCodec>());
        engine.addPlugin<client::RTypeNetworkPlugin>(connection, [this] {
            ++transportsMade;
            transport = std::make_shared<FakeClient>();
            transport->udpOpens = udpOpens;
            transport->connects = connects;
            return transport;
        });
        engine.addPlugin<client::PlayerControlPlugin>();
        auto& bus = engine.eventBus();
        bus.subscribe<client::SessionConnected>([this](const auto& /*event*/) { results.emplace_back("connected"); });
        bus.subscribe<client::SessionClosed>([this](const client::SessionClosed& event) {
            results.push_back(std::string(event.duringConnection ? "failed: " : "closed: ") + event.reason);
        });
        bus.subscribe<client::RoomListUpdated>([this](const auto& /*event*/) { results.emplace_back("rooms"); });
        bus.subscribe<client::RoomUpdated>([this](const client::RoomUpdated& event) {
            results.push_back("room " + std::to_string(event.room.roomId));
        });
        bus.subscribe<client::RoomLeft>([this](const auto& /*event*/) { results.emplace_back("left"); });
        bus.subscribe<client::GameStarted>([this](const client::GameStarted& event) {
            results.push_back("game " + std::to_string(event.playerIndex) + "/" + std::to_string(event.playerCount));
        });
        bus.subscribe<client::LevelStarted>(
            [this](const client::LevelStarted& event) { results.push_back("level " + event.level.name); });
        bus.subscribe<client::LevelCompleted>([this](const client::LevelCompleted& event) {
            results.push_back("cleared " + std::to_string(event.result.levelIndex));
        });
        bus.subscribe<client::GameEnded>([this](const auto& /*event*/) { results.emplace_back("over"); });
        engine.init();
        inputEntity = engine.registry().spawnEntity();
        engine.registry().addComponent(inputEntity, ::input::InputState{.playerIndex = 0, .actions = {}});
    }

    [[nodiscard]] client::RTypeNetworkPlugin& session() { return engine.getPlugin<client::RTypeNetworkPlugin>(); }

    /** @brief Lets one fixed step of time elapse, and runs the engine. */
    void step() {
        now += static_cast<double>(DT);
        engine.advance();
    }

    /** @brief The server sends a message; the client handles it during the frame this runs. */
    void receive(const protocol::Message& message, const Channel channel = Channel::Reliable) {
        transport->next.push_back({.type = EventType::Data,
                                   .peer = ecs::net::SERVER_PEER,
                                   .data = protocol::encode(message),
                                   .channel = channel});
        step();
    }

    /** @brief The transport reports an event (connected, disconnected). */
    void transportEvent(const EventType type) {
        transport->next.push_back({.type = type, .peer = 0, .data = {}, .channel = Channel::Reliable});
        step();
    }

    /** @brief Connects and gets the `Welcome`: the client is in the lobby. */
    void connect() {
        engine.eventBus().publish(client::ConnectRequest{.host = "server", .port = 4000});
        transportEvent(EventType::Connected);
        receive(protocol::Welcome{.udpPort = UDP_PORT});
    }

    /** @brief Connects, then enters a room. */
    void enterRoom() {
        connect();
        receive(protocol::LobbyUpdate{.roomId = ROOM, .slots = {{.clientId = 1, .ready = false}}});
    }

    /** @brief Connects, enters a room, and the game starts. */
    void startGame(const std::uint8_t playerCount = 1) {
        enterRoom();
        receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = playerCount});
    }

    /** @brief Messages of one type the client sent on the current transport, oldest first. */
    template <typename Message>
    [[nodiscard]] std::vector<Message> sentMessages() const {
        std::vector<Message> messages;
        if (!transport) {
            return messages;
        }
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
        step();
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

    [[nodiscard]] std::size_t replicatedCount() {
        return engine.registry().getResource<ecs::net::NetworkEntityMap>().size();
    }

    double now = 0.0;
    bool udpOpens = true;
    bool connects = true;
    int transportsMade = 0;
    std::shared_ptr<FakeClient> transport;
    std::vector<std::string> results;
    engine::Engine engine{[this]() -> double { return now; }};
    ecs::Entity inputEntity = 0;
};

}  // namespace

// --- Session ---------------------------------------------------------------------------------------------------------

TEST_F(RTypeNetworkPluginTest, DoesNothingUntilAskedToConnect) {
    step();
    EXPECT_EQ(transportsMade, 0);
    EXPECT_EQ(session().state(), client::SessionState::Offline);
}

TEST_F(RTypeNetworkPluginTest, ConnectsSaysHelloAndReachesTheLobby) {
    engine.eventBus().publish(client::ConnectRequest{.host = "server", .port = 4000});
    EXPECT_EQ(transport->connectedTo, "server:4000");
    EXPECT_EQ(session().state(), client::SessionState::Connecting);

    transportEvent(EventType::Connected);
    ASSERT_EQ(sentMessages<protocol::Hello>().size(), 1U);
    EXPECT_EQ(sentMessages<protocol::Hello>()[0].version, protocol::PROTOCOL_VERSION);
    EXPECT_EQ(transport->sent[0].channel, Channel::Reliable);

    receive(protocol::Welcome{.udpPort = UDP_PORT});
    EXPECT_EQ(session().state(), client::SessionState::Lobby);
    EXPECT_EQ(sentMessages<protocol::LobbyListRequest>().size(), 1U);
    EXPECT_FALSE(transport->udpPort.has_value()) << "the RFC opens UDP at GameStart";
    EXPECT_EQ(results, (std::vector<std::string>{"connected"}));
}

TEST_F(RTypeNetworkPluginTest, IgnoresMalformedAndUnreliableMessages) {
    engine.eventBus().publish(client::ConnectRequest{.host = "server", .port = 4000});
    transport->next.push_back({.type = EventType::Data, .peer = 0, .data = {0xFF, 0x13}, .channel = Channel::Reliable});
    receive(protocol::Welcome{.udpPort = UDP_PORT}, Channel::Unreliable);

    EXPECT_TRUE(sentMessages<protocol::LobbyListRequest>().empty()) << "Welcome is only valid on the reliable channel";
    EXPECT_EQ(session().state(), client::SessionState::Connecting);
}

TEST_F(RTypeNetworkPluginTest, AFailedConnectionIsReportedAndCanBeRetried) {
    engine.eventBus().publish(client::ConnectRequest{.host = "nowhere", .port = 1});
    transport->reason = ecs::net::DisconnectReason::Unreachable;
    transportEvent(EventType::Disconnected);
    EXPECT_EQ(results, (std::vector<std::string>{"failed: the server cannot be reached"}));
    EXPECT_EQ(session().state(), client::SessionState::Offline);
    EXPECT_EQ(session().lastError(), "the server cannot be reached");
    EXPECT_FALSE(engine.isRunning()) << "the engine was never stopped (nor started)";

    connect();
    EXPECT_EQ(transportsMade, 2) << "a transport connects once: each attempt has a new one";
    EXPECT_EQ(session().state(), client::SessionState::Lobby);
}

TEST_F(RTypeNetworkPluginTest, ALostConnectionClosesTheGameButNotTheEngine) {
    startGame();
    receive(twoShips(1), Channel::Unreliable);
    ASSERT_EQ(replicatedCount(), 2U);

    transportEvent(EventType::Disconnected);
    EXPECT_EQ(results.back(), "closed: connection to the server lost");
    EXPECT_EQ(session().state(), client::SessionState::Offline);
    EXPECT_EQ(replicatedCount(), 0U);
    EXPECT_FALSE(session().playerIndex().has_value());
}

TEST_F(RTypeNetworkPluginTest, ClosesTheSessionAndSendsNoInputWhenUdpCannotBeOpened) {
    udpOpens = false;
    enterRoom();
    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 1});
    step();

    EXPECT_EQ(results.back(), "closed: cannot open UDP on port 4242");
    EXPECT_EQ(session().state(), client::SessionState::Offline);
    EXPECT_TRUE(sentMessages<protocol::PlayerInput>().empty()) << "the game must not start without UDP";
}

TEST_F(RTypeNetworkPluginTest, DisconnectRequestLeavesAndStopsSilently) {
    enterRoom();
    engine.eventBus().publish(client::DisconnectRequest{});
    EXPECT_EQ(sentMessages<protocol::Leave>().size(), 1U);
    EXPECT_TRUE(transport->stopped);
    EXPECT_EQ(session().state(), client::SessionState::Offline);
    EXPECT_EQ(std::ranges::count_if(results, [](const std::string& r) { return r.starts_with("closed"); }), 0);
}

TEST_F(RTypeNetworkPluginTest, LeavesOnShutdown) {
    connect();
    engine.shutdown();

    EXPECT_EQ(sentMessages<protocol::Leave>().size(), 1U);
}

// --- Lobby -----------------------------------------------------------------------------------------------------------

TEST_F(RTypeNetworkPluginTest, LobbyRequestsAreSentOnlyInTheRightState) {
    engine.eventBus().publish(client::CreateRoomRequest{});  // offline: ignored
    connect();
    auto& bus = engine.eventBus();
    bus.publish(client::SetReadyRequest{.ready = true});  // in no room: ignored
    bus.publish(client::JoinRoomRequest{.roomId = ROOM});
    bus.publish(client::CreateRoomRequest{});
    bus.publish(client::RefreshRoomsRequest{});

    ASSERT_EQ(sentMessages<protocol::LobbyJoin>().size(), 1U);
    EXPECT_EQ(sentMessages<protocol::LobbyJoin>()[0].roomId, ROOM);
    EXPECT_EQ(sentMessages<protocol::LobbyCreate>().size(), 1U);
    EXPECT_EQ(sentMessages<protocol::LobbyListRequest>().size(), 2U);
    EXPECT_TRUE(sentMessages<protocol::LobbyReady>().empty());

    receive(protocol::LobbyUpdate{.roomId = ROOM, .slots = {{.clientId = 1, .ready = false}}});
    bus.publish(client::SetReadyRequest{.ready = true});
    bus.publish(client::JoinRoomRequest{.roomId = 99});  // already in a room: ignored
    ASSERT_EQ(sentMessages<protocol::LobbyReady>().size(), 1U);
    EXPECT_TRUE(sentMessages<protocol::LobbyReady>()[0].ready);
    EXPECT_EQ(sentMessages<protocol::LobbyJoin>().size(), 1U);
}

TEST_F(RTypeNetworkPluginTest, KeepsTheLastListAndRoomForScreensOpenedLater) {
    connect();
    receive(protocol::LobbyList{
        .rooms = {{.roomId = ROOM, .playerCount = 1, .maxPlayers = 4, .state = protocol::RoomState::Waiting}}});
    ASSERT_EQ(session().rooms().rooms.size(), 1U);
    receive(protocol::LobbyUpdate{.roomId = ROOM, .slots = {{.clientId = 1, .ready = true}}});
    ASSERT_TRUE(session().room().has_value());
    EXPECT_EQ(session().room()->roomId, ROOM);
    EXPECT_EQ(session().state(), client::SessionState::InRoom);
    EXPECT_EQ(results, (std::vector<std::string>{"connected", "rooms", "room 7"}));
}

TEST_F(RTypeNetworkPluginTest, LeavingARoomGoesBackToTheLobby) {
    enterRoom();
    engine.eventBus().publish(client::LeaveRoomRequest{});
    EXPECT_EQ(sentMessages<protocol::Leave>().size(), 1U);
    EXPECT_EQ(session().state(), client::SessionState::Lobby);
    EXPECT_FALSE(session().room().has_value());
    EXPECT_EQ(results.back(), "left");
}

// --- In game ---------------------------------------------------------------------------------------------------------

TEST_F(RTypeNetworkPluginTest, GameStartOpensUdpAndReportsTheLevels) {
    startGame(2);
    EXPECT_EQ(transport->udpPort, UDP_PORT);
    EXPECT_EQ(session().state(), client::SessionState::InGame);
    EXPECT_EQ(session().playerIndex(), 0);

    receive(protocol::LevelStart{.levelIndex = 0, .levelCount = 2, .name = "Outer Rim"});
    receive(protocol::LevelComplete{.levelIndex = 0, .score = 3, .playerScores = {1, 2}});
    ASSERT_TRUE(session().level().has_value());
    EXPECT_EQ(session().level()->name, "Outer Rim");
    EXPECT_EQ(results, (std::vector<std::string>{"connected", "room 7", "game 0/2", "level Outer Rim", "cleared 0"}));
}

TEST_F(RTypeNetworkPluginTest, GameOverGoesBackToTheLobbyWithACleanWorld) {
    startGame(2);
    receive(twoShips(40), Channel::Unreliable);
    ASSERT_EQ(replicatedCount(), 2U);

    receive(protocol::GameOver{.reason = protocol::GameOverReason::Victory, .score = 9, .playerScores = {4, 5}});
    EXPECT_EQ(results.back(), "over");
    EXPECT_EQ(session().state(), client::SessionState::Lobby);
    EXPECT_EQ(transport->udpClosed, 1);
    EXPECT_EQ(replicatedCount(), 0U);
    ASSERT_TRUE(session().lastResult().has_value());
    EXPECT_EQ(session().lastResult()->score, 9U);

    // A second game, in a new room whose ticks start again from 1: its snapshots are not dropped as stale.
    receive(protocol::LobbyUpdate{.roomId = ROOM + 1, .slots = {{.clientId = 1, .ready = true}}});
    receive(protocol::GameStart{.roomId = ROOM + 1, .playerIndex = 0, .playerCount = 2});
    receive(twoShips(1), Channel::Unreliable);
    EXPECT_EQ(replicatedCount(), 2U);
}

TEST_F(RTypeNetworkPluginTest, SendsNoInputBeforeTheGameStartsNorAfterItEnds) {
    enterRoom();
    step();
    EXPECT_TRUE(sentMessages<protocol::PlayerInput>().empty());

    receive(protocol::GameStart{.roomId = ROOM, .playerIndex = 0, .playerCount = 1});
    step();
    EXPECT_FALSE(sentMessages<protocol::PlayerInput>().empty());

    receive(protocol::GameOver{.reason = protocol::GameOverReason::AllDead, .score = 0, .playerScores = {0}});
    transport->sent.clear();
    step();
    step();
    EXPECT_TRUE(sentMessages<protocol::PlayerInput>().empty());
}

TEST_F(RTypeNetworkPluginTest, APausedPlayerSendsNoInput) {
    startGame();
    engine.getPlugin<client::PlayerControlPlugin>().setPaused(true);
    transport->sent.clear();
    step();
    step();
    EXPECT_TRUE(sentMessages<protocol::PlayerInput>().empty());
    engine.getPlugin<client::PlayerControlPlugin>().setPaused(false);
    step();
    EXPECT_FALSE(sentMessages<protocol::PlayerInput>().empty());
}

TEST_F(RTypeNetworkPluginTest, SendsTheActionsOfEachStepOverUdp) {
    startGame();
    transport->sent.clear();
    actions()[game::ACTION_MOVE_X].axis = -1.0F;
    actions()[::input::makeActionId("shoot")].held = true;
    actions()[client::input::ACTION_READY].held = true;
    actions()[::input::makeActionId("charge_weapon")].held = false;

    // Each step also sends the inputs not acknowledged yet: the last one sent is the input of the step.
    step();
    const auto first = sentMessages<protocol::PlayerInput>().back();
    step();
    const auto second = sentMessages<protocol::PlayerInput>().back();

    EXPECT_EQ(second.sequence, first.sequence + 1);
    EXPECT_EQ(first.held, std::vector<protocol::ActionId>{::input::makeActionId("shoot")})
        << "ready is not a game input";
    ASSERT_EQ(first.axes.size(), 1U);
    EXPECT_EQ(first.axes[0].action, game::ACTION_MOVE_X);
    EXPECT_FLOAT_EQ(first.axes[0].value, -1.0F);
    EXPECT_EQ(transport->sent[0].channel, Channel::Unreliable);
}

TEST_F(RTypeNetworkPluginTest, LocalShipIsPredictedAndTheOthersInterpolated) {
    startGame(2);
    receive(twoShips(1), Channel::Unreliable);

    auto& registry = engine.registry();
    EXPECT_TRUE(registry.hasComponent<ecs::Predicted>(entityOf(MY_SHIP)));
    EXPECT_FALSE(registry.hasComponent<ecs::Predicted>(entityOf(OTHER_SHIP)));
    EXPECT_TRUE(registry.hasComponent<ecs::Interpolated>(entityOf(OTHER_SHIP)));
}

TEST_F(RTypeNetworkPluginTest, LocalShipMovesAtOnceAndOnlyIt) {
    startGame(2);
    receive(twoShips(1), Channel::Unreliable);
    actions()[game::ACTION_MOVE_X].axis = 1.0F;
    const auto& transforms = engine.registry().getComponents<ecs::Transform>();
    const float mine = transforms.get(entityOf(MY_SHIP)).x;
    const float other = transforms.get(entityOf(OTHER_SHIP)).x;

    step();

    EXPECT_FLOAT_EQ(transforms.get(entityOf(MY_SHIP)).x, mine + (game::SHIP_SPEED * DT));
    EXPECT_FLOAT_EQ(transforms.get(entityOf(OTHER_SHIP)).x, other);
}

// --- Automatic lobby -------------------------------------------------------------------------------------------------

// --- Command line ----------------------------------------------------------------------------------------------------

namespace {

/** @brief Runs `parseOptions()` on arguments given as strings. */
std::optional<client::ClientOptions> parse(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }
    return client::parseOptions(argv);
}

}  // namespace

TEST(ClientOptions, HasDefaults) {
    const auto options = parse({"r-type_client"});

    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->host, "127.0.0.1");
    EXPECT_EQ(options->port, protocol::DEFAULT_PORT);
}

TEST(ClientOptions, ReadsTheHostAndThePort) {
    const auto options = parse({"r-type_client", "example.org", "5000"});

    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->host, "example.org");
    EXPECT_EQ(options->port, 5000);
}

TEST(ClientOptions, RefusesAnInvalidPortOrTooManyArguments) {
    EXPECT_FALSE(parse({"r-type_client", "host", "0"}).has_value());
    EXPECT_FALSE(parse({"r-type_client", "host", "65536"}).has_value());
    EXPECT_FALSE(parse({"r-type_client", "host", "42abc"}).has_value());
    EXPECT_FALSE(parse({"r-type_client", "host", "1", "extra"}).has_value());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
