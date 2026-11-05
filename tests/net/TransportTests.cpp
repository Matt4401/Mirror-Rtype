#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <asio/buffer.hpp>
#include <asio/connect.hpp>  // IWYU pragma: keep
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
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
#include <vector>

#include "net/INetworkClient.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkConfig.hpp"
#include "net/NetworkFactory.hpp"

namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
using ecs::net::NetworkConfig;
using ecs::net::NetworkEvent;

constexpr std::uint32_t GAME_A = 0xA;
constexpr std::uint32_t GAME_B = 0xB;
constexpr auto WAIT = std::chrono::seconds(2);

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
    constexpr std::uint16_t PORT = 47311;
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
    constexpr std::uint16_t PORT = 47312;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));
    const auto client = ecs::net::createClient({.protocolId = GAME_B});
    ASSERT_TRUE(client->connect("127.0.0.1", PORT));

    const auto clientEvents = pollUntil(*client, [](const auto& e) { return has(e, EventType::Disconnected); });
    EXPECT_TRUE(has(clientEvents, EventType::Disconnected));
    EXPECT_FALSE(has(clientEvents, EventType::Connected));

    // The server never announced the peer: no Connected, and no Disconnected either.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(server->poll().empty());
}

TEST(Transport, FrameBeforeHandshakeClosesTheConnection) {
    constexpr std::uint16_t PORT = 47313;
    const auto server = ecs::net::createServer({.protocolId = GAME_A});
    ASSERT_TRUE(server->start(PORT));

    // A Data frame (kind 0) with an empty payload: `[length u32 = 0][kind u8 = 0]`.
    EXPECT_TRUE(serverClosesRawConnection(PORT, Bytes{0, 0, 0, 0, 0}));
    EXPECT_TRUE(server->poll().empty());
}

TEST(Transport, SilentConnectionIsClosedAfterHandshakeTimeout) {
    constexpr std::uint16_t PORT = 47314;
    const auto server = ecs::net::createServer({.protocolId = GAME_A,
                                                .udpTick = std::chrono::milliseconds(20),
                                                .handshakeTimeout = std::chrono::milliseconds(100)});
    ASSERT_TRUE(server->start(PORT));

    EXPECT_TRUE(serverClosesRawConnection(PORT, {}));
    EXPECT_TRUE(server->poll().empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
