#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <asio/buffer.hpp>
#include <asio/connect.hpp>  // IWYU pragma: keep
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/socket_base.hpp>
#include <asio/write.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkConfig.hpp"
#include "net/NetworkFactory.hpp"

namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::DisconnectReason;
using ecs::net::EventType;
using ecs::net::NetworkConfig;
using ecs::net::NetworkEvent;

constexpr std::uint32_t GAME_A = 0xA;
constexpr std::uint32_t GAME_B = 0xB;
// The ports of these tests are below every ephemeral range (from 32768 on Linux, 49152 on Windows and macOS). Inside
// it, an outgoing connection can get a test port as its source port and hold it in TIME_WAIT: start() then fails for a
// minute with "Address already in use", and the tests themselves open such connections.
// Long enough for Windows, where connecting to a closed port takes about 2 s: the SYN is retried after each refusal.
constexpr auto WAIT = std::chrono::seconds(10);
/** @brief What "at once" means for an operation that must not wait for the network. */
constexpr auto PROMPTLY = std::chrono::seconds(2);

/** @brief Polls a transport until `done` holds or `WAIT` elapses; returns every event seen. */
std::vector<NetworkEvent> pollUntil(ecs::net::INetworkManager& net,
                                    const std::function<bool(const std::vector<NetworkEvent>&)>& done) {
    std::vector<NetworkEvent> seen;
    const auto deadline = std::chrono::steady_clock::now() + WAIT;
    while (!done(seen) && std::chrono::steady_clock::now() < deadline) {
        for (auto& event : net.poll()) {
            seen.push_back(std::move(event));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return seen;
}

bool has(const std::vector<NetworkEvent>& events, const EventType type) {
    return std::ranges::any_of(events, [type](const NetworkEvent& e) { return e.type == type; });
}

/** @brief Opens a raw TCP connection and returns whether the server closed it within `WAIT`. */
bool serverClosesRawConnection(const std::uint16_t port, const Bytes& firstBytes) {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    asio::ip::tcp::resolver resolver(io);
    // NOLINTNEXTLINE(misc-include-cleaner): false positive, the public header asio/connect.hpp is included
    asio::connect(socket, resolver.resolve("127.0.0.1", std::to_string(port)));
    if (!firstBytes.empty()) {
        asio::write(socket, asio::buffer(firstBytes));
    }
    auto closed = std::async(std::launch::async, [&socket] {
        std::array<std::uint8_t, 64> buffer{};
        std::error_code ec;
        while (!ec) {
            socket.read_some(asio::buffer(buffer), ec);
        }
        return true;
    });
    if (closed.wait_for(WAIT) == std::future_status::ready) {
        return closed.get();
    }
    std::error_code ignored;
    socket.close(ignored);  // unblocks read_some
    return false;
}

/**
 * @brief Returns whether `stop()` ends within `WAIT`. A stop that hangs leaves its thread and the server behind, on
 * purpose: nothing could join them, and the failure must be reported instead of hanging the whole test binary.
 */
bool stopsInTime(std::shared_ptr<ecs::net::INetworkServer> server) {
    auto stopped = std::make_shared<std::promise<void>>();
    auto future = stopped->get_future();
    std::thread([server = std::move(server), stopped] {
        server->stop();
        stopped->set_value();
    }).detach();
    return future.wait_for(WAIT) == std::future_status::ready;
}

}  // namespace

TEST(NetworkConfig, DefaultIsValid) { EXPECT_TRUE(ecs::net::isValid(NetworkConfig{})); }

TEST(NetworkConfig, InvalidConfigIsRefusedByTheFactory) {
    EXPECT_FALSE(ecs::net::isValid(NetworkConfig{.maxPeers = 0}));
    EXPECT_FALSE(ecs::net::isValid(NetworkConfig{.keepaliveInterval = std::chrono::seconds(10)}));  // >= udpTimeout
    EXPECT_THROW((void)ecs::net::createServer(NetworkConfig{.maxPeers = 0}), std::invalid_argument);
    EXPECT_THROW((void)ecs::net::createClient(NetworkConfig{.udpTick = std::chrono::milliseconds(0)}),
                 std::invalid_argument);
}

TEST(Transport, SameProtocolConnectsAndExchangesData) {
    constexpr std::uint16_t PORT = 21311;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));
    const auto client = ecs::net::createClient({.protocolId = GAME_A});
    ASSERT_TRUE(client->connect("127.0.0.1", PORT));

    const auto clientEvents = pollUntil(*client, [](const auto& e) { return has(e, EventType::Connected); });
    ASSERT_TRUE(has(clientEvents, EventType::Connected));
    const auto serverEvents = pollUntil(*server, [](const auto& e) { return has(e, EventType::Connected); });
    ASSERT_TRUE(has(serverEvents, EventType::Connected));
    const ecs::net::PeerId peer = serverEvents.front().peer;
    EXPECT_NE(peer, 0U);

    client->send(Bytes{1, 2, 3}, Channel::Reliable);
    const auto received = pollUntil(*server, [](const auto& e) { return has(e, EventType::Data); });
    ASSERT_TRUE(has(received, EventType::Data));
    EXPECT_EQ(received.back().data, (Bytes{1, 2, 3}));
    EXPECT_EQ(received.back().peer, peer);
}

TEST(Transport, OtherProtocolIsRefusedBeforeTheGameSeesIt) {
    constexpr std::uint16_t PORT = 21312;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));
    const auto client = ecs::net::createClient({.protocolId = GAME_B});
    ASSERT_TRUE(client->connect("127.0.0.1", PORT));

    const auto clientEvents = pollUntil(*client, [](const auto& e) { return has(e, EventType::Disconnected); });
    EXPECT_TRUE(has(clientEvents, EventType::Disconnected));
    EXPECT_FALSE(has(clientEvents, EventType::Connected));
    EXPECT_EQ(client->disconnectReason(), DisconnectReason::ProtocolMismatch);

    // The server never announced the peer: no Connected, and no Disconnected either.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(server->poll().empty());
}

TEST(Transport, FrameBeforeHandshakeClosesTheConnection) {
    constexpr std::uint16_t PORT = 21313;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));

    // A Data frame (kind 0) with an empty payload: `[length u32 = 0][kind u8 = 0]`.
    EXPECT_TRUE(serverClosesRawConnection(PORT, Bytes{0, 0, 0, 0, 0}));
    EXPECT_TRUE(server->poll().empty());
}

TEST(Transport, SilentConnectionIsClosedAfterHandshakeTimeout) {
    constexpr std::uint16_t PORT = 21314;
    const auto server = ecs::net::createServer({.protocolId = GAME_A,
                                                .udpTick = std::chrono::milliseconds(20),
                                                .handshakeTimeout = std::chrono::milliseconds(100)});
    ASSERT_TRUE(server->start(PORT));

    EXPECT_TRUE(serverClosesRawConnection(PORT, {}));
    EXPECT_TRUE(server->poll().empty());
}

TEST(Transport, FullServerTellsTheClientWhy) {
    constexpr std::uint16_t PORT = 21315;
    const auto server = ecs::net::createServer({.protocolId = GAME_A, .maxPeers = 1});
    ASSERT_TRUE(server->start(PORT));
    const auto first = ecs::net::createClient({.protocolId = GAME_A});
    ASSERT_TRUE(first->connect("127.0.0.1", PORT));
    ASSERT_TRUE(
        has(pollUntil(*first, [](const auto& e) { return has(e, EventType::Connected); }), EventType::Connected));

    const auto second = ecs::net::createClient({.protocolId = GAME_A});
    ASSERT_TRUE(second->connect("127.0.0.1", PORT));

    const auto events = pollUntil(*second, [](const auto& e) { return has(e, EventType::Disconnected); });
    EXPECT_TRUE(has(events, EventType::Disconnected));
    EXPECT_FALSE(has(events, EventType::Connected));
    EXPECT_EQ(second->disconnectReason(), DisconnectReason::ServerFull);
}

TEST(Transport, RefusedPeerThatStaysConnectedIsClosedAnyway) {
    constexpr std::uint16_t PORT = 21322;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    asio::ip::tcp::resolver resolver(io);
    // NOLINTNEXTLINE(misc-include-cleaner): false positive, the public header asio/connect.hpp is included
    asio::connect(socket, resolver.resolve("127.0.0.1", std::to_string(PORT)));

    // A `Handshake` of another protocol: `[length u32 = 4][kind u8 = 4][protocolId u32 = 0xB]`. Then this peer stays
    // silent and never closes.
    asio::write(socket, asio::buffer(Bytes{4, 0, 0, 0, 4, GAME_B, 0, 0, 0}));
    std::array<std::uint8_t, 6> refused{};
    asio::read(socket, asio::buffer(refused));
    EXPECT_EQ(refused[4], 5U) << "kind of a Refused frame";
    EXPECT_EQ(refused[5], static_cast<std::uint8_t>(DisconnectReason::ProtocolMismatch));

    const auto before = std::chrono::steady_clock::now();
    server->stop();
    EXPECT_LT(std::chrono::steady_clock::now() - before, PROMPTLY) << "the server does not wait for this peer";
}

TEST(Transport, RefusedPeerLearnsTheReasonAfterConnecting) {
    constexpr std::uint16_t PORT = 21316;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));
    const auto client = ecs::net::createClient({.protocolId = GAME_A});
    ASSERT_TRUE(client->connect("127.0.0.1", PORT));
    const auto serverEvents = pollUntil(*server, [](const auto& e) { return has(e, EventType::Connected); });
    ASSERT_TRUE(has(serverEvents, EventType::Connected));

    server->refuse(serverEvents.front().peer, DisconnectReason::ProtocolMismatch);

    const auto events = pollUntil(*client, [](const auto& e) { return has(e, EventType::Disconnected); });
    EXPECT_TRUE(has(events, EventType::Disconnected));
    EXPECT_EQ(client->disconnectReason(), DisconnectReason::ProtocolMismatch);
}

TEST(Transport, LostConnectionHasNoSpecialReason) {
    constexpr std::uint16_t PORT = 21317;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));
    const auto client = ecs::net::createClient({.protocolId = GAME_A});
    ASSERT_TRUE(client->connect("127.0.0.1", PORT));
    const auto serverEvents = pollUntil(*server, [](const auto& e) { return has(e, EventType::Connected); });
    ASSERT_TRUE(has(serverEvents, EventType::Connected));

    server->kick(serverEvents.front().peer);

    const auto events = pollUntil(*client, [](const auto& e) { return has(e, EventType::Disconnected); });
    EXPECT_TRUE(has(events, EventType::Disconnected));
    EXPECT_EQ(client->disconnectReason(), DisconnectReason::ConnectionLost);
}

TEST(Transport, UnreachableServerIsReportedByAnEvent) {
    constexpr std::uint16_t CLOSED_PORT = 21318;
    const auto client = ecs::net::createClient({.protocolId = GAME_A});

    ASSERT_TRUE(client->connect("127.0.0.1", CLOSED_PORT));

    const auto events = pollUntil(*client, [](const auto& e) { return has(e, EventType::Disconnected); });
    EXPECT_TRUE(has(events, EventType::Disconnected));
    EXPECT_FALSE(has(events, EventType::Connected));
    EXPECT_EQ(client->disconnectReason(), DisconnectReason::Unreachable);
    EXPECT_FALSE(client->isRunning());
}

TEST(Transport, ConnectDoesNotBlockAndCanBeCancelled) {
    // Reserved for documentation (RFC 5737): nothing answers there, so the attempt stays pending or fails on its own.
    const std::string blackHole = "192.0.2.1";
    const auto client = ecs::net::createClient({.protocolId = GAME_A});

    const auto before = std::chrono::steady_clock::now();
    ASSERT_TRUE(client->connect(blackHole, 21319));
    client->send(Bytes{1}, Channel::Reliable);  // dropped: not connected yet
    client->stop();

    EXPECT_LT(std::chrono::steady_clock::now() - before, PROMPTLY);
}

TEST(Transport, StopRightAfterConnectNeverHangs) {
    constexpr std::uint16_t PORT = 21321;
    constexpr int ATTEMPTS = 200;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));

    // stop() can run before, during or after the resolution and the connection: every order must let it join.
    auto stopped = std::async(std::launch::async, [] {
        for (int attempt = 0; attempt < ATTEMPTS; ++attempt) {
            const auto client = ecs::net::createClient({.protocolId = GAME_A});
            client->connect("127.0.0.1", PORT);
            if (attempt % 2 == 0) {
                std::this_thread::yield();
            }
            client->stop();
        }
    });

    ASSERT_EQ(stopped.wait_for(std::chrono::seconds(30)), std::future_status::ready);
    stopped.get();
}

TEST(Transport, ServerStopDuringAConnectionBurstNeverHangs) {
    constexpr std::uint16_t PORT = 21323;
    constexpr int ATTEMPTS = 100;
    constexpr int CONNECTING_THREADS = 4;
    std::atomic<bool> done{false};
    std::vector<std::future<void>> bursts;
    for (int thread = 0; thread < CONNECTING_THREADS; ++thread) {
        bursts.push_back(std::async(std::launch::async, [&done] {
            const asio::ip::tcp::endpoint server(asio::ip::make_address_v4("127.0.0.1"), PORT);
            while (!done) {
                asio::io_context io;
                asio::ip::tcp::socket socket(io, asio::ip::tcp::v4());
                // Closes with a reset: a normal close keeps the ephemeral port in TIME_WAIT for a minute, and thousands
                // of them would use up the range and collide with the ports of other test suites. The close must be
                // explicit, because the destructor of an asio socket turns the linger option off.
                socket.set_option(asio::socket_base::linger(true, 0));
                std::error_code ignored;
                std::ignore = socket.connect(server, ignored);
                std::ignore = socket.close(ignored);
            }
        }));
    }

    // A connection accepted just before stop() completes after the acceptor is closed.
    int started = 0;
    bool stopped = true;
    for (int attempt = 0; attempt < ATTEMPTS && stopped; ++attempt) {
        std::shared_ptr server = ecs::net::createServer({.protocolId = GAME_A});
        if (server->start(PORT)) {
            ++started;
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            stopped = stopsInTime(std::move(server));
        }
    }
    done = true;

    EXPECT_TRUE(stopped);
    EXPECT_GT(started, 0);
}

TEST(Transport, ServerStopOnATimerTickNeverHangs) {
    constexpr std::uint16_t PORT = 21324;
    constexpr int ATTEMPTS = 100;

    // A timer that has just expired cannot be cancelled anymore: stop() must also hold when its handler runs
    // afterwards. The delays go across the 1 ms tick, to stop as close to its expiry as possible.
    for (int attempt = 0; attempt < ATTEMPTS; ++attempt) {
        std::shared_ptr server =
            ecs::net::createServer({.protocolId = GAME_A, .udpTick = std::chrono::milliseconds(1)});
        ASSERT_TRUE(server->start(PORT));
        std::this_thread::sleep_for(std::chrono::microseconds(900 + (attempt * 3)));
        ASSERT_TRUE(stopsInTime(std::move(server)));
    }
}

TEST(Transport, ConnectingTwiceIsRefused) {
    constexpr std::uint16_t PORT = 21320;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));
    const auto client = ecs::net::createClient({.protocolId = GAME_A});

    EXPECT_TRUE(client->connect("127.0.0.1", PORT));
    EXPECT_FALSE(client->connect("127.0.0.1", PORT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
