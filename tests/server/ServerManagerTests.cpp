#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "Network/INetworkManager.hpp"
#include "Network/INetworkServer.hpp"
#include "ServerManager.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace {

using rtype::net::Bytes;
using rtype::net::Channel;
using rtype::net::ClientId;
using rtype::net::EventType;
using rtype::net::NetworkEvent;
using rtype::server::ServerManager;
namespace protocol = rtype::protocol;

constexpr std::uint16_t PORT = 4242;
constexpr auto TIMEOUT = std::chrono::seconds(2);

/** @brief One payload sent by the server. */
struct Sent {
    ClientId client;
    Bytes data;
    Channel channel;
};

/** @brief State shared between the test and the fake network owned by the server (game thread). */
struct NetworkState {
    std::mutex mutex;
    std::vector<NetworkEvent> incoming;
    std::vector<Sent> sent;
    std::vector<ClientId> kicked;
};

/** @brief In-memory network: the test pushes events, the server polls them and its sends are recorded. */
class FakeNetwork : public rtype::net::server::INetworkServer {
  public:
    explicit FakeNetwork(std::shared_ptr<NetworkState> state) : _state(std::move(state)) {}

    bool start(std::uint16_t /*port*/) override { return true; }
    void stop() override {}
    [[nodiscard]] bool isRunning() const override { return true; }

    std::vector<NetworkEvent> poll() override {
        const std::scoped_lock lock(_state->mutex);
        return std::exchange(_state->incoming, {});
    }
    void send(const ClientId client, const Bytes& data, const Channel channel) override {
        const std::scoped_lock lock(_state->mutex);
        _state->sent.push_back({.client = client, .data = data, .channel = channel});
    }
    void broadcast(const Bytes& /*data*/, Channel /*channel*/) override {}
    void kick(const ClientId client) override {
        const std::scoped_lock lock(_state->mutex);
        _state->kicked.push_back(client);
    }

  private:
    std::shared_ptr<NetworkState> _state;
};

/** @brief Runs a real `ServerManager` loop on its own thread, on top of the fake network. */
class ServerManagerTest : public ::testing::Test {
  protected:
    ServerManagerTest()
        : state(std::make_shared<NetworkState>()),
          server(std::make_unique<FakeNetwork>(state)),
          loop([this] { server.run(PORT); }) {}

    ~ServerManagerTest() override {
        server.stop();
        loop.join();
    }

    ServerManagerTest(const ServerManagerTest&) = delete;
    ServerManagerTest& operator=(const ServerManagerTest&) = delete;
    ServerManagerTest(ServerManagerTest&&) = delete;
    ServerManagerTest& operator=(ServerManagerTest&&) = delete;

    void push(const EventType type, const ClientId client, Bytes data = {}, const Channel channel = Channel::Reliable) {
        const std::scoped_lock lock(state->mutex);
        state->incoming.push_back({.type = type, .client = client, .data = std::move(data), .channel = channel});
    }

    void message(const ClientId client, const protocol::Message& msg, const Channel channel = Channel::Reliable) {
        push(EventType::Data, client, protocol::encode(msg), channel);
    }

    /** @brief Waits until `done` holds on the shared state, or the timeout expires. */
    bool waitFor(const std::function<bool(const NetworkState&)>& done) {
        const auto deadline = std::chrono::steady_clock::now() + TIMEOUT;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                const std::scoped_lock lock(state->mutex);
                if (done(*state)) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }

    /** @brief Number of `Welcome` messages sent to a client. Call with the mutex held (inside `waitFor`). */
    static std::size_t welcomes(const NetworkState& network, const ClientId client) {
        return static_cast<std::size_t>(std::ranges::count_if(network.sent, [client](const Sent& sent) {
            const auto decoded = protocol::decode(sent.data);
            return sent.client == client && decoded && std::holds_alternative<protocol::Welcome>(*decoded);
        }));
    }

    /**
     * @brief Waits until every event pushed so far has been handled.
     *
     * Events are handled in order, so once a fresh client's `Hello` is answered, the earlier events are done.
     */
    void sync() {
        const ClientId sentinel = _nextSentinel++;
        message(sentinel, protocol::Hello{});
        ASSERT_TRUE(waitFor([sentinel](const NetworkState& network) { return welcomes(network, sentinel) == 1; }));
    }

    std::shared_ptr<NetworkState> state;
    ServerManager server;
    std::thread loop;

  private:
    ClientId _nextSentinel = 1000;
};

constexpr ClientId CLIENT = 1;

}  // namespace

TEST_F(ServerManagerTest, HelloIsAnsweredWithWelcome) {
    message(CLIENT, protocol::Hello{});
    ASSERT_TRUE(waitFor([](const NetworkState& network) { return welcomes(network, CLIENT) == 1; }));

    const std::scoped_lock lock(state->mutex);
    const auto& sent = state->sent.front();
    EXPECT_EQ(sent.channel, Channel::Reliable);
    EXPECT_EQ(protocol::decode(sent.data), protocol::Message{protocol::Welcome{.udpPort = PORT}});
}

TEST_F(ServerManagerTest, WrongVersionIsKicked) {
    message(CLIENT, protocol::Hello{.version = protocol::PROTOCOL_VERSION + 1});
    ASSERT_TRUE(waitFor([](const NetworkState& network) { return !network.kicked.empty(); }));
    sync();

    const std::scoped_lock lock(state->mutex);
    EXPECT_EQ(state->kicked, std::vector<ClientId>{CLIENT});
    EXPECT_EQ(welcomes(*state, CLIENT), 0U);
}

TEST_F(ServerManagerTest, SecondHelloIsDropped) {
    message(CLIENT, protocol::Hello{});
    message(CLIENT, protocol::Hello{});
    sync();

    const std::scoped_lock lock(state->mutex);
    EXPECT_EQ(welcomes(*state, CLIENT), 1U);
}

TEST_F(ServerManagerTest, HelloOverUdpIsDropped) {
    message(CLIENT, protocol::Hello{}, Channel::Unreliable);
    sync();

    const std::scoped_lock lock(state->mutex);
    EXPECT_EQ(welcomes(*state, CLIENT), 0U);
}

TEST_F(ServerManagerTest, MessagesBeforeHelloDoNotCompleteTheHandshake) {
    message(CLIENT, protocol::LobbyCreate{});
    message(CLIENT, protocol::PlayerInput{.sequence = 1, .buttons = protocol::BUTTON_UP}, Channel::Unreliable);
    message(CLIENT, protocol::Leave{});
    sync();
    {
        const std::scoped_lock lock(state->mutex);
        EXPECT_EQ(welcomes(*state, CLIENT), 0U);
    }

    // The client can still do the handshake afterwards.
    message(CLIENT, protocol::Hello{});
    EXPECT_TRUE(waitFor([](const NetworkState& network) { return welcomes(network, CLIENT) == 1; }));
}

TEST_F(ServerManagerTest, DisconnectionResetsTheHandshake) {
    message(CLIENT, protocol::Hello{});
    push(EventType::Disconnected, CLIENT);
    // Network ids can be reused by a new connection: it must do its own handshake.
    push(EventType::Connected, CLIENT);
    message(CLIENT, protocol::Hello{});
    EXPECT_TRUE(waitFor([](const NetworkState& network) { return welcomes(network, CLIENT) == 2; }));
}

TEST_F(ServerManagerTest, InvalidPayloadIsIgnored) {
    push(EventType::Data, CLIENT, Bytes{0xFF, 0x00, 0x13});
    push(EventType::Data, CLIENT, Bytes{}, Channel::Unreliable);
    message(CLIENT, protocol::Hello{});
    EXPECT_TRUE(waitFor([](const NetworkState& network) { return welcomes(network, CLIENT) == 1; }));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
