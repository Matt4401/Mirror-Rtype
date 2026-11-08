#include <gtest/gtest.h>

#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <variant>

#include "ServerManager.hpp"
#include "components/Predicted.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "input/InputState.hpp"
#include "input/RTypeBindings.hpp"
#include "metrics/MetricsBoard.hpp"
#include "metrics/MetricsPlugin.hpp"
#include "metrics/ShipMetricsPlugin.hpp"
#include "net/ClientConnection.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkFactory.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PlayerControlPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/SessionEvents.hpp"
#include "net/metrics/NetworkMetricsPlugin.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

// A real ServerManager and real clients, talking over loopback sockets: everything the other suites replace with fakes.
namespace {

using ecs::net::Channel;
using rtype::server::ServerManager;
namespace protocol = rtype::protocol;
namespace game = rtype::game;

constexpr std::uint16_t PORT = 45871;
constexpr auto DEADLINE = std::chrono::seconds(10);

/** @brief The client of the game, without its window: same network plugins, and the test plays the input devices. */
class GameClient {
  public:
    GameClient() : _connection(std::make_shared<ecs::net::ClientConnection>(nullptr)) {
        engine.addPlugin<ecs::net::NetworkPlugin>(_connection, _connection);
        engine.addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<game::ReplicationCodec>());
        engine.addPlugin<client::RTypeNetworkPlugin>(
            _connection, [] { return ecs::net::createClient({.protocolId = protocol::TRANSPORT_PROTOCOL_ID}); });
        engine.addPlugin<client::PlayerControlPlugin>();
        engine.addPlugin<metrics::MetricsPlugin>();
        engine.addPlugin<ecs::net::NetworkMetricsPlugin>([this] { return _connection->stats(); });
        engine.addPlugin<client::ShipMetricsPlugin>();
        engine.eventBus().subscribe<ecs::net::PacketReceived>([this](const ecs::net::PacketReceived& packet) {
            const auto message = protocol::decode(packet.data);
            if (packet.channel != Channel::Reliable || !message) {
                return;
            }
            if (const auto* update = std::get_if<protocol::LobbyUpdate>(&*message)) {
                lobbyUpdate = *update;
            } else if (const auto* start = std::get_if<protocol::GameStart>(&*message)) {
                gameStart = *start;
            } else if (const auto* left = std::get_if<protocol::PlayerLeft>(&*message)) {
                playerLeft = *left;
            }
        });
        engine.init();
        _input = engine.registry().spawnEntity();
        engine.registry().addComponent(_input, ::input::InputState{.playerIndex = 0, .actions = {}});
    }

    /** @brief Asks the session to connect to the local server, as the connect screen does. */
    [[nodiscard]] bool connect() {
        engine.eventBus().publish(client::ConnectRequest{.host = "127.0.0.1", .port = PORT});
        return session().state() == client::SessionState::Connecting;
    }

    /** @brief The session of this client. */
    [[nodiscard]] client::RTypeNetworkPlugin& session() { return engine.getPlugin<client::RTypeNetworkPlugin>(); }

    /** @brief Publishes a request, as a screen of the client does. */
    template <typename Request>
    void request(const Request& request) {
        engine.eventBus().publish(request);
    }

    /** @brief Holds the horizontal axis. */
    void setMoveX(const float value) { actions()[game::ACTION_MOVE_X].axis = value; }

    /** @brief Position of the ship of a player, as this client sees it. */
    [[nodiscard]] std::optional<ecs::Transform> shipOf(const std::uint8_t playerIndex) {
        std::optional<ecs::Transform> found;
        engine.registry().runSystem<ecs::Replicated, game::EntityNetState, ecs::Transform>(
            [&](ecs::Entity /*entity*/, const ecs::Replicated& replicated, const game::EntityNetState& state,
                const ecs::Transform& transform) {
                if (replicated.kind == game::KIND_PLAYER_SHIP && state.subtype == playerIndex) {
                    found = transform;
                }
            });
        return found;
    }

    /** @brief Value of a line of the metrics board, empty if it does not exist yet. */
    [[nodiscard]] std::string metric(const std::string_view name) {
        const metrics::Metric* line = engine.registry().getResource<metrics::MetricsBoard>().find(name);
        return line == nullptr ? std::string{} : line->value;
    }

    /** @brief Number of replicated entities in this client's world. */
    [[nodiscard]] std::size_t entityCount() {
        return engine.registry().getResource<ecs::net::NetworkEntityMap>().size();
    }

    engine::Engine engine;
    std::optional<protocol::LobbyUpdate> lobbyUpdate;
    std::optional<protocol::GameStart> gameStart;
    std::optional<protocol::PlayerLeft> playerLeft;

  private:
    std::unordered_map<::input::ActionId, ::input::ActionData>& actions() {
        return engine.registry().getComponents<::input::InputState>().get(_input).actions;
    }

    std::shared_ptr<ecs::net::ClientConnection> _connection;
    ecs::Entity _input = 0;
};

/** @brief Runs the real server on its own thread, with the real transport. */
class EndToEndTest : public ::testing::Test {
  protected:
    EndToEndTest()
        : server(ecs::net::createServer({.protocolId = protocol::TRANSPORT_PROTOCOL_ID})),
          loop([this] { server.run(PORT); }) {}

    ~EndToEndTest() override {
        server.stop();
        loop.join();
    }

    EndToEndTest(const EndToEndTest&) = delete;
    EndToEndTest& operator=(const EndToEndTest&) = delete;
    EndToEndTest(EndToEndTest&&) = delete;
    EndToEndTest& operator=(EndToEndTest&&) = delete;

    /** @brief Waits until the server thread has bound its port, then connects: a client that failed cannot retry. */
    static bool connect(GameClient& client) {
        asio::io_context io;
        const asio::ip::tcp::endpoint server(asio::ip::make_address("127.0.0.1"), PORT);
        const auto deadline = std::chrono::steady_clock::now() + DEADLINE;
        std::error_code error;
        do {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            asio::ip::tcp::socket probe(io);
            std::ignore = probe.connect(server, error);
        } while (error);
        return client.connect();
    }

    /** @brief Runs the clients' engines until `done` holds, or the deadline expires. */
    static bool pump(const std::function<void()>& frame, const std::function<bool()>& done) {
        const auto deadline = std::chrono::steady_clock::now() + DEADLINE;
        while (!done()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    ServerManager server;
    std::thread loop;
};

}  // namespace

TEST_F(EndToEndTest, TwoClientsPlayTogetherAndSurviveACrash) {
    GameClient first;
    auto second = std::make_unique<GameClient>();
    const auto frame = [&] {
        first.engine.advance();
        if (second) {
            second->engine.advance();
        }
    };

    // Lobby: the first client creates a room, the second sees it in the list and joins it, both get ready.
    ASSERT_TRUE(connect(first));
    ASSERT_TRUE(pump(frame, [&] { return first.session().state() == client::SessionState::Lobby; }));
    first.request(client::CreateRoomRequest{});
    ASSERT_TRUE(pump(frame, [&] { return first.lobbyUpdate.has_value(); }));
    ASSERT_TRUE(connect(*second));
    ASSERT_TRUE(pump(frame, [&] { return !second->session().rooms().rooms.empty(); }));
    second->request(client::JoinRoomRequest{.roomId = second->session().rooms().rooms.front().roomId});
    ASSERT_TRUE(pump(frame, [&] { return second->lobbyUpdate.has_value(); }));
    EXPECT_EQ(second->lobbyUpdate->roomId, first.lobbyUpdate->roomId);
    first.request(client::SetReadyRequest{.ready = true});
    second->request(client::SetReadyRequest{.ready = true});
    ASSERT_TRUE(pump(frame, [&] { return first.gameStart && second->gameStart; }));
    EXPECT_EQ(first.gameStart->playerIndex, 0);
    EXPECT_EQ(second->gameStart->playerIndex, 1);
    EXPECT_EQ(first.gameStart->playerCount, 2);

    // Snapshots over UDP: each client now sees both ships.
    ASSERT_TRUE(pump(frame, [&] { return first.entityCount() == 2 && second->entityCount() == 2; }));
    const float spawnX = first.shipOf(0)->x;

    // The first client moves: its ship moves at once, without waiting for the server.
    first.setMoveX(1.0F);
    ASSERT_TRUE(pump(frame, [&] { return first.shipOf(0)->x > spawnX + 100.0F; }));
    first.setMoveX(0.0F);

    // Everyone converges on the server's position: reconciliation must not have moved the ship anywhere else.
    EXPECT_TRUE(pump(frame,
                     [&] {
                         return second->shipOf(0)->x > spawnX + 100.0F &&
                                std::abs(first.shipOf(0)->x - second->shipOf(0)->x) < 0.01F;
                     }))
        << "first sees x=" << first.shipOf(0)->x << ", second sees x=" << second->shipOf(0)->x;
    EXPECT_FLOAT_EQ(first.shipOf(1)->x, second->shipOf(1)->x) << "the ship that did not move is the same everywhere";

    // The second client crashes: the server survives, and the first client keeps playing without its ship.
    second.reset();
    ASSERT_TRUE(pump(frame, [&] { return first.playerLeft.has_value() && first.entityCount() == 1; }));
    EXPECT_FALSE(first.shipOf(1).has_value());
    const auto tick = first.engine.registry().getResource<ecs::net::SnapshotState>().tick;
    EXPECT_TRUE(pump(frame, [&] { return first.engine.registry().getResource<ecs::net::SnapshotState>().tick > tick; }))
        << "snapshots keep coming";
}

TEST_F(EndToEndTest, ClientMeasuresItsConnection) {
    GameClient client;
    const auto frame = [&] { client.engine.advance(); };
    ASSERT_TRUE(connect(client));
    ASSERT_TRUE(pump(frame, [&] { return client.session().state() == client::SessionState::Lobby; }));
    client.request(client::CreateRoomRequest{});
    ASSERT_TRUE(pump(frame, [&] { return client.lobbyUpdate.has_value(); }));
    client.request(client::SetReadyRequest{.ready = true});
    ASSERT_TRUE(pump(frame, [&] { return client.gameStart.has_value(); }));

    // Through real sockets: inputs get acknowledged, snapshots come in, and all of it reaches the board.
    EXPECT_TRUE(pump(frame, [&] {
        const std::string rtt = client.metric("RTT");
        return !rtt.empty() && rtt != "--";
    })) << "no input was acknowledged";
    EXPECT_TRUE(pump(frame, [&] {
        const std::string down = client.metric("Down");
        return !down.empty() && down != "0.0 KB/s";
    })) << "no traffic was measured";
    EXPECT_TRUE(pump(frame, [&] { return !client.metric("Position").empty(); })) << "the game adds its own metric";
    EXPECT_GT(client.engine.registry().getResource<ecs::net::ReplicationStats>().snapshotsReceived, 0U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
