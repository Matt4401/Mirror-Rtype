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
#include <system_error>
#include <thread>
#include <tuple>
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
#include "net/ClientConnection.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkFactory.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationEvents.hpp"
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

/** @brief What a game client does: one engine, the network plugins, and the lobby handled on `PacketReceived`. */
class GameClient {
  public:
    GameClient() : _connection(std::make_shared<ecs::net::ClientConnection>(_transport)) {
        engine.addPlugin<ecs::net::NetworkPlugin>(_connection, _connection);
        engine.addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<game::ReplicationCodec>());

        engine.eventBus().subscribe<ecs::net::PeerConnected>(
            [this](const ecs::net::PeerConnected& /*event*/) { sendMessage(protocol::Hello{}); });
        engine.eventBus().subscribe<ecs::net::PacketReceived>(
            [this](const ecs::net::PacketReceived& packet) { onPacket(packet); });
        engine.eventBus().subscribe<ecs::net::ReplicatedEntitySpawned>(
            [this](const ecs::net::ReplicatedEntitySpawned& /*event*/) { markLocalShip(); });
        engine.eventBus().subscribe<ecs::net::InputApplied>([this](const ecs::net::InputApplied& event) {
            game::applyShipInput(engine.registry().getComponents<ecs::Transform>().get(event.entity), event.input,
                                 event.deltaTime);
        });
        engine.init();
    }

    /** @brief Connects to the local server. */
    [[nodiscard]] bool connect() { return _transport->connect("127.0.0.1", PORT); }

    void sendMessage(const protocol::Message& message) {
        _transport->send(protocol::encode(message), Channel::Reliable);
    }

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

    /** @brief Sequence of the last input of this client the server said it applied. */
    [[nodiscard]] std::uint32_t acknowledged() {
        return engine.registry().getResource<ecs::net::SnapshotState>().lastProcessedInput;
    }

    engine::Engine engine;
    std::optional<protocol::LobbyUpdate> lobbyUpdate;
    std::optional<protocol::GameStart> gameStart;
    std::optional<protocol::PlayerLeft> playerLeft;

  private:
    /**
     * @brief The local ship predicts; every other entity just follows the snapshots.
     *
     * Called on both events: a snapshot (UDP) can overtake `GameStart` (TCP), so either can come first.
     */
    void markLocalShip() {
        if (!gameStart) {
            return;
        }
        ecs::Registry& registry = engine.registry();
        std::optional<ecs::Entity> local;
        registry.runSystem<game::EntityNetState>([&](const ecs::Entity entity, const game::EntityNetState& state) {
            if (state.subtype == gameStart->playerIndex) {
                local = entity;
            }
        });
        if (local && !registry.hasComponent<ecs::Predicted>(*local)) {
            registry.addComponent(*local, ecs::Predicted{});
        }
    }

    void onPacket(const ecs::net::PacketReceived& packet) {
        const auto message = protocol::decode(packet.data);
        if (!message) {
            return;
        }
        if (const auto* welcome = std::get_if<protocol::Welcome>(&*message)) {
            _transport->openUdp(welcome->udpPort);
        } else if (const auto* update = std::get_if<protocol::LobbyUpdate>(&*message)) {
            lobbyUpdate = *update;
        } else if (const auto* start = std::get_if<protocol::GameStart>(&*message)) {
            gameStart = *start;
            markLocalShip();
        } else if (const auto* left = std::get_if<protocol::PlayerLeft>(&*message)) {
            playerLeft = *left;
        }
    }

    std::shared_ptr<ecs::net::INetworkClient> _transport =
        ecs::net::createClient({.protocolId = protocol::TRANSPORT_PROTOCOL_ID});
    std::shared_ptr<ecs::net::ClientConnection> _connection;
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
    ASSERT_TRUE(connect(first));
    ASSERT_TRUE(connect(*second));
    const auto frame = [&] {
        first.engine.advance();
        if (second) {
            second->engine.advance();
        }
    };

    // Lobby: the first client creates a room, the second joins it, both get ready.
    ASSERT_TRUE(pump(frame, [&] {
        first.sendMessage(protocol::LobbyCreate{});  // dropped until Hello is answered, and once in the room
        return first.lobbyUpdate.has_value();
    }));
    ASSERT_TRUE(pump(frame, [&] {
        second->sendMessage(protocol::LobbyJoin{.roomId = first.lobbyUpdate->roomId});
        return second->lobbyUpdate.has_value();
    }));
    first.sendMessage(protocol::LobbyReady{.ready = true});
    second->sendMessage(protocol::LobbyReady{.ready = true});
    ASSERT_TRUE(pump(frame, [&] { return first.gameStart && second->gameStart; }));
    EXPECT_EQ(first.gameStart->playerIndex, 0);
    EXPECT_EQ(second->gameStart->playerIndex, 1);
    EXPECT_EQ(first.gameStart->playerCount, 2);

    // Snapshots over UDP: each client now sees both ships.
    ASSERT_TRUE(pump(frame, [&] { return first.entityCount() == 2 && second->entityCount() == 2; }));
    const float spawnX = first.shipOf(0)->x;

    // The first client moves: one input per fixed step, sent over UDP and predicted locally.
    constexpr std::uint32_t INPUTS = 30;
    std::uint32_t sequence = 0;
    ASSERT_TRUE(pump(
        [&] {
            if (first.engine.advance() > 0 && sequence < INPUTS) {
                first.engine.eventBus().publish(ecs::net::LocalInput{
                    .input = {
                        .sequence = ++sequence, .held = {}, .axes = {{.action = game::ACTION_MOVE_X, .value = 1.0F}}}});
            }
            second->engine.advance();
        },
        [&] { return sequence == INPUTS; }));
    EXPECT_GT(first.shipOf(0)->x, spawnX) << "the local ship moves at once, without waiting for the server";

    // Everyone converges on the server's position, and reconciliation did not change the predicted result.
    EXPECT_TRUE(pump(
        frame,
        [&] { return first.acknowledged() == INPUTS && std::abs(first.shipOf(0)->x - second->shipOf(0)->x) < 0.01F; }))
        << "acknowledged " << first.acknowledged() << ", first sees x=" << first.shipOf(0)->x
        << ", second sees x=" << second->shipOf(0)->x;
    EXPECT_NEAR(first.shipOf(0)->x,
                spawnX + (static_cast<float>(INPUTS) * game::SHIP_SPEED * first.engine.fixedDeltaTime()), 0.01F);
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
