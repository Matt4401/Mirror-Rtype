#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
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
#include "net/ClientConnection.hpp"
#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkFactory.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/ReplicationPlugins.hpp"
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
    GameClient() : _connection(std::make_shared<engine::ClientConnection>(_transport)) {
        engine.addPlugin<ecs::net::NetworkPlugin>(_connection, _connection);
        engine.addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<game::ReplicationCodec>());
        engine.addPlugin<client::RTypeNetworkPlugin>(_transport);
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

    /** @brief Connects to the local server. */
    [[nodiscard]] bool connect() { return _transport->connect("127.0.0.1", PORT); }

    /** @brief Presses the ready key for one frame. */
    void pressReady() {
        actions()[client::input::ACTION_READY].justPressed = true;
        engine.advance();
        actions()[client::input::ACTION_READY].justPressed = false;
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

    std::shared_ptr<ecs::net::INetworkClient> _transport =
        ecs::net::createClient({.protocolId = protocol::TRANSPORT_PROTOCOL_ID});
    std::shared_ptr<engine::ClientConnection> _connection;
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

    /** @brief Connects a client, retrying while the server thread has not bound its port yet. */
    static bool connect(GameClient& client) {
        const auto deadline = std::chrono::steady_clock::now() + DEADLINE;
        while (!client.connect()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
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

    // Lobby: the first client finds no room and creates one, the second finds it and joins it, both get ready.
    ASSERT_TRUE(connect(first));
    ASSERT_TRUE(pump(frame, [&] { return first.lobbyUpdate.has_value(); }));
    ASSERT_TRUE(connect(*second));
    ASSERT_TRUE(pump(frame, [&] { return second->lobbyUpdate.has_value(); }));
    EXPECT_EQ(second->lobbyUpdate->roomId, first.lobbyUpdate->roomId);
    first.pressReady();
    second->pressReady();
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
