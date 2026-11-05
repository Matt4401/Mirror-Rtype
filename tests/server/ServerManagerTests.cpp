#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "ServerManager.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
using ecs::net::NetworkEvent;
using ClientId = ecs::net::PeerId;
using rtype::server::ServerManager;
namespace protocol = rtype::protocol;

constexpr std::uint16_t PORT = 4242;
/** @brief Any action id: the server forwards inputs without knowing what their actions mean. */
constexpr protocol::ActionId SHOOT = 0x5EED;
constexpr auto TIMEOUT = std::chrono::seconds(2);
/** @brief Handshake timeout of `HandshakeTimeoutTest`, short so that the tests stay fast. */
constexpr auto SHORT_HANDSHAKE_TIMEOUT = std::chrono::milliseconds(50);

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
class FakeNetwork : public ecs::net::INetworkServer {
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
    ServerManagerTest() : ServerManagerTest(rtype::server::HANDSHAKE_TIMEOUT) {}

    explicit ServerManagerTest(const std::chrono::milliseconds handshakeTimeout)
        : state(std::make_shared<NetworkState>()),
          server(std::make_unique<FakeNetwork>(state), handshakeTimeout),
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
        state->incoming.push_back({.type = type, .peer = client, .data = std::move(data), .channel = channel});
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

    /** @brief Messages of one type sent to a client, oldest first. Call with the mutex held (inside `waitFor`). */
    template <typename Message>
    static std::vector<Message> received(const NetworkState& network, const ClientId client) {
        std::vector<Message> messages;
        for (const Sent& sent : network.sent) {
            const auto decoded = protocol::decode(sent.data);
            if (sent.client == client && decoded && std::holds_alternative<Message>(*decoded)) {
                messages.push_back(std::get<Message>(*decoded));
            }
        }
        return messages;
    }

    /** @brief Waits until a client received at least `count` messages of one type, and returns the last one. */
    template <typename Message>
    std::optional<Message> waitForMessage(const ClientId client, const std::size_t count = 1) {
        std::optional<Message> last;
        waitFor([&](const NetworkState& network) {
            const auto messages = received<Message>(network, client);
            if (messages.size() < count) {
                return false;
            }
            last = messages.back();
            return true;
        });
        return last;
    }

    /** @brief Connects a client and waits for its `Welcome`. */
    void connect(const ClientId client) {
        message(client, protocol::Hello{});
        ASSERT_TRUE(waitFor([client](const NetworkState& network) { return welcomes(network, client) == 1; }));
    }

    /** @brief Creates a room with `owner` in it, and returns its id. */
    rtype::server::RoomId createRoom(const ClientId owner) {
        message(owner, protocol::LobbyCreate{});
        const auto update = waitForMessage<protocol::LobbyUpdate>(owner);
        return update ? update->roomId : 0;
    }

    /** @brief Number of messages of one type sent to a client so far. */
    template <typename Message>
    std::size_t countOf(const ClientId client) {
        const std::scoped_lock lock(state->mutex);
        return received<Message>(*state, client).size();
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

/** @brief Same server, with a handshake timeout short enough to expire during a test. */
class HandshakeTimeoutTest : public ServerManagerTest {
  protected:
    HandshakeTimeoutTest() : ServerManagerTest(SHORT_HANDSHAKE_TIMEOUT) {}

    /** @brief Lets the handshake timeout expire several times over, then waits for the pending events. */
    void waitPastTimeout() {
        std::this_thread::sleep_for(SHORT_HANDSHAKE_TIMEOUT * 4);
        sync();
    }
};

constexpr ClientId CLIENT = 1;
constexpr ClientId OTHER = 2;
constexpr ClientId THIRD = 3;

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
    message(CLIENT, protocol::PlayerInput{.sequence = 1, .held = {SHOOT}, .axes = {}}, Channel::Unreliable);
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

TEST_F(HandshakeTimeoutTest, SilentClientIsKickedOnce) {
    push(EventType::Connected, CLIENT);
    ASSERT_TRUE(waitFor([](const NetworkState& network) { return !network.kicked.empty(); }));
    // The fake network never sends Disconnected: the server must not kick the client again on every iteration.
    waitPastTimeout();

    const std::scoped_lock lock(state->mutex);
    EXPECT_EQ(state->kicked, std::vector<ClientId>{CLIENT});
}

TEST_F(HandshakeTimeoutTest, MessagesOtherThanHelloDoNotStopTheTimeout) {
    push(EventType::Connected, CLIENT);
    message(CLIENT, protocol::LobbyListRequest{});
    message(CLIENT, protocol::PlayerInput{.sequence = 1, .held = {SHOOT}, .axes = {}}, Channel::Unreliable);
    EXPECT_TRUE(waitFor([](const NetworkState& network) { return !network.kicked.empty(); }));
}

TEST_F(HandshakeTimeoutTest, HelloInTimeIsNotKicked) {
    push(EventType::Connected, CLIENT);
    message(CLIENT, protocol::Hello{});
    ASSERT_TRUE(waitFor([](const NetworkState& network) { return welcomes(network, CLIENT) == 1; }));
    waitPastTimeout();

    const std::scoped_lock lock(state->mutex);
    EXPECT_TRUE(state->kicked.empty());
}

TEST_F(HandshakeTimeoutTest, WrongVersionIsKickedOnlyOnce) {
    push(EventType::Connected, CLIENT);
    message(CLIENT, protocol::Hello{.version = protocol::PROTOCOL_VERSION + 1});
    waitPastTimeout();

    const std::scoped_lock lock(state->mutex);
    EXPECT_EQ(state->kicked, std::vector<ClientId>{CLIENT});
}

TEST_F(HandshakeTimeoutTest, DisconnectionCancelsTheTimeout) {
    push(EventType::Connected, CLIENT);
    push(EventType::Disconnected, CLIENT);
    waitPastTimeout();

    const std::scoped_lock lock(state->mutex);
    EXPECT_TRUE(state->kicked.empty());
}

// --- Lobby -----------------------------------------------------------------------------------------------------------

TEST_F(ServerManagerTest, ListIsEmptyWithoutRooms) {
    connect(CLIENT);
    message(CLIENT, protocol::LobbyListRequest{});

    const auto list = waitForMessage<protocol::LobbyList>(CLIENT);
    ASSERT_TRUE(list.has_value());
    EXPECT_TRUE(list->rooms.empty());
}

TEST_F(ServerManagerTest, LobbyMessagesBeforeHelloAreDropped) {
    message(CLIENT, protocol::LobbyCreate{});
    message(CLIENT, protocol::LobbyListRequest{});
    sync();

    EXPECT_EQ(countOf<protocol::LobbyUpdate>(CLIENT), 0U);
    EXPECT_EQ(countOf<protocol::LobbyList>(CLIENT), 0U);
}

TEST_F(ServerManagerTest, CreatingARoomPutsTheClientInIt) {
    connect(CLIENT);
    connect(OTHER);
    message(CLIENT, protocol::LobbyCreate{});

    const auto update = waitForMessage<protocol::LobbyUpdate>(CLIENT);
    ASSERT_TRUE(update.has_value());
    EXPECT_NE(update->roomId, 0U);
    EXPECT_EQ(update->slots, (std::vector<protocol::PlayerSlot>{{.clientId = CLIENT, .ready = false}}));

    const auto list = waitForMessage<protocol::LobbyList>(OTHER);
    ASSERT_TRUE(list.has_value()) << "every client in the lobby is told";
    ASSERT_EQ(list->rooms.size(), 1U);
    EXPECT_EQ(list->rooms[0], (protocol::RoomEntry{.roomId = update->roomId,
                                                   .playerCount = 1,
                                                   .maxPlayers = rtype::server::MAX_PLAYERS_PER_ROOM,
                                                   .state = protocol::RoomState::Waiting}));
}

TEST_F(ServerManagerTest, ClientInARoomCannotCreateOrJoinAnother) {
    connect(CLIENT);
    connect(OTHER);
    const auto first = createRoom(CLIENT);
    const auto second = createRoom(OTHER);

    message(CLIENT, protocol::LobbyCreate{});
    message(CLIENT, protocol::LobbyJoin{.roomId = second});
    sync();

    EXPECT_EQ(countOf<protocol::LobbyUpdate>(CLIENT), 1U);
    EXPECT_NE(first, second);
}

TEST_F(ServerManagerTest, JoinKeepsTheJoinOrderAndTellsTheRoom) {
    connect(CLIENT);
    connect(OTHER);
    const auto id = createRoom(CLIENT);

    message(OTHER, protocol::LobbyJoin{.roomId = id});

    const std::vector<protocol::PlayerSlot> expected{{.clientId = CLIENT, .ready = false},
                                                     {.clientId = OTHER, .ready = false}};
    const auto forOther = waitForMessage<protocol::LobbyUpdate>(OTHER);
    ASSERT_TRUE(forOther.has_value());
    EXPECT_EQ(forOther->slots, expected);
    const auto forOwner = waitForMessage<protocol::LobbyUpdate>(CLIENT, 2);
    ASSERT_TRUE(forOwner.has_value());
    EXPECT_EQ(forOwner->slots, expected);
}

TEST_F(ServerManagerTest, JoiningAnUnknownRoomIsIgnored) {
    connect(CLIENT);
    message(CLIENT, protocol::LobbyJoin{.roomId = 999});
    sync();

    EXPECT_EQ(countOf<protocol::LobbyUpdate>(CLIENT), 0U);
}

TEST_F(ServerManagerTest, JoiningAFullRoomIsIgnored) {
    const ClientId late = 50;
    connect(CLIENT);
    const auto id = createRoom(CLIENT);
    for (ClientId client = 10; client < 10 + rtype::server::MAX_PLAYERS_PER_ROOM - 1; ++client) {
        connect(client);
        message(client, protocol::LobbyJoin{.roomId = id});
        ASSERT_TRUE(waitForMessage<protocol::LobbyUpdate>(client).has_value());
    }

    connect(late);
    message(late, protocol::LobbyJoin{.roomId = id});
    sync();

    EXPECT_EQ(countOf<protocol::LobbyUpdate>(late), 0U);
}

TEST_F(ServerManagerTest, LobbyMessagesOverUdpAreDropped) {
    connect(CLIENT);
    message(CLIENT, protocol::LobbyCreate{}, Channel::Unreliable);
    sync();

    EXPECT_EQ(countOf<protocol::LobbyUpdate>(CLIENT), 0U);
}

TEST_F(ServerManagerTest, ReadyOutsideARoomIsDropped) {
    connect(CLIENT);
    message(CLIENT, protocol::LobbyReady{.ready = true});
    sync();

    EXPECT_EQ(countOf<protocol::LobbyUpdate>(CLIENT), 0U);
    EXPECT_EQ(countOf<protocol::GameStart>(CLIENT), 0U);
}

TEST_F(ServerManagerTest, GameStartsWhenEveryoneIsReady) {
    connect(CLIENT);
    connect(OTHER);
    const auto id = createRoom(CLIENT);
    message(OTHER, protocol::LobbyJoin{.roomId = id});
    ASSERT_TRUE(waitForMessage<protocol::LobbyUpdate>(OTHER).has_value());

    message(CLIENT, protocol::LobbyReady{.ready = true});
    const auto update = waitForMessage<protocol::LobbyUpdate>(OTHER, 2);
    ASSERT_TRUE(update.has_value());
    EXPECT_TRUE(update->slots[0].ready);
    EXPECT_FALSE(update->slots[1].ready);
    EXPECT_EQ(countOf<protocol::GameStart>(CLIENT), 0U);

    message(OTHER, protocol::LobbyReady{.ready = true});
    const auto first = waitForMessage<protocol::GameStart>(CLIENT);
    const auto second = waitForMessage<protocol::GameStart>(OTHER);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, (protocol::GameStart{.roomId = id, .playerIndex = 0, .playerCount = 2}));
    EXPECT_EQ(*second, (protocol::GameStart{.roomId = id, .playerIndex = 1, .playerCount = 2}));
}

TEST_F(ServerManagerTest, CancellingReadyPreventsTheStart) {
    connect(CLIENT);
    connect(OTHER);
    const auto id = createRoom(CLIENT);
    message(OTHER, protocol::LobbyJoin{.roomId = id});
    message(CLIENT, protocol::LobbyReady{.ready = true});
    message(CLIENT, protocol::LobbyReady{.ready = false});
    message(OTHER, protocol::LobbyReady{.ready = true});
    sync();

    EXPECT_EQ(countOf<protocol::GameStart>(CLIENT), 0U);
    EXPECT_EQ(countOf<protocol::GameStart>(OTHER), 0U);
}

TEST_F(ServerManagerTest, StartedRoomIsListedInGameAndCannotBeJoined) {
    connect(CLIENT);
    connect(OTHER);
    const auto id = createRoom(CLIENT);
    message(CLIENT, protocol::LobbyReady{.ready = true});
    ASSERT_TRUE(waitForMessage<protocol::GameStart>(CLIENT).has_value());

    message(OTHER, protocol::LobbyJoin{.roomId = id});
    message(OTHER, protocol::LobbyListRequest{});
    sync();

    EXPECT_EQ(countOf<protocol::LobbyUpdate>(OTHER), 0U);
    const auto list = waitForMessage<protocol::LobbyList>(OTHER);
    ASSERT_TRUE(list.has_value());
    ASSERT_EQ(list->rooms.size(), 1U);
    EXPECT_EQ(list->rooms[0].state, protocol::RoomState::InGame);
}

TEST_F(ServerManagerTest, LeavingTellsTheOthersAndDestroysTheEmptyRoom) {
    connect(CLIENT);
    connect(OTHER);
    connect(THIRD);
    const auto id = createRoom(CLIENT);
    message(OTHER, protocol::LobbyJoin{.roomId = id});
    ASSERT_TRUE(waitForMessage<protocol::LobbyUpdate>(OTHER).has_value());

    message(OTHER, protocol::Leave{});
    const auto left = waitForMessage<protocol::PlayerLeft>(CLIENT);
    ASSERT_TRUE(left.has_value());
    EXPECT_EQ(left->clientId, OTHER);
    const auto update = waitForMessage<protocol::LobbyUpdate>(CLIENT, 3);
    ASSERT_TRUE(update.has_value());
    EXPECT_EQ(update->slots, (std::vector<protocol::PlayerSlot>{{.clientId = CLIENT, .ready = false}}));

    message(CLIENT, protocol::Leave{});
    sync();
    message(THIRD, protocol::LobbyListRequest{});
    sync();
    const std::scoped_lock lock(state->mutex);
    EXPECT_TRUE(received<protocol::LobbyList>(*state, THIRD).back().rooms.empty());
}

TEST_F(ServerManagerTest, DepartureOfTheLastUnreadyPlayerStartsTheGame) {
    connect(CLIENT);
    connect(OTHER);
    const auto id = createRoom(CLIENT);
    message(OTHER, protocol::LobbyJoin{.roomId = id});
    message(CLIENT, protocol::LobbyReady{.ready = true});
    ASSERT_TRUE(waitForMessage<protocol::LobbyUpdate>(OTHER, 2).has_value());

    push(EventType::Disconnected, OTHER);

    const auto start = waitForMessage<protocol::GameStart>(CLIENT);
    ASSERT_TRUE(start.has_value());
    EXPECT_EQ(*start, (protocol::GameStart{.roomId = id, .playerIndex = 0, .playerCount = 1}));
}

// --- In game ---------------------------------------------------------------------------------------------------------

TEST_F(ServerManagerTest, StartedRoomSendsSnapshotsAndAppliesInputs) {
    connect(CLIENT);
    createRoom(CLIENT);
    message(CLIENT, protocol::PlayerInput{.sequence = 1, .held = {}, .axes = {}}, Channel::Unreliable);
    message(CLIENT, protocol::LobbyReady{.ready = true});
    ASSERT_TRUE(waitForMessage<protocol::GameStart>(CLIENT).has_value());

    const auto first = waitForMessage<protocol::WorldState>(CLIENT);
    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(first->entities.size(), 1U);
    EXPECT_EQ(first->lastProcessedInput, 0U) << "the input sent before the start is dropped";

    message(CLIENT, protocol::PlayerInput{.sequence = 5, .held = {SHOOT}, .axes = {}}, Channel::Unreliable);
    EXPECT_TRUE(waitFor([](const NetworkState& network) {
        const auto snapshots = received<protocol::WorldState>(network, CLIENT);
        return !snapshots.empty() && snapshots.back().lastProcessedInput == 5;
    }));
}

TEST_F(ServerManagerTest, LobbyMessagesInGameAreDropped) {
    connect(CLIENT);
    createRoom(CLIENT);
    message(CLIENT, protocol::LobbyReady{.ready = true});
    ASSERT_TRUE(waitForMessage<protocol::GameStart>(CLIENT).has_value());
    const auto updates = countOf<protocol::LobbyUpdate>(CLIENT);
    const auto lists = countOf<protocol::LobbyList>(CLIENT);

    message(CLIENT, protocol::LobbyReady{.ready = false});
    message(CLIENT, protocol::LobbyCreate{});
    message(CLIENT, protocol::LobbyListRequest{});
    sync();

    EXPECT_EQ(countOf<protocol::LobbyUpdate>(CLIENT), updates);
    EXPECT_EQ(countOf<protocol::LobbyList>(CLIENT), lists);
    EXPECT_EQ(countOf<protocol::GameStart>(CLIENT), 1U);
}

TEST_F(ServerManagerTest, SnapshotsStayInTheirRoom) {
    connect(CLIENT);
    connect(OTHER);
    createRoom(CLIENT);
    createRoom(OTHER);
    message(CLIENT, protocol::LobbyReady{.ready = true});

    ASSERT_TRUE(waitForMessage<protocol::WorldState>(CLIENT).has_value());
    EXPECT_EQ(countOf<protocol::WorldState>(OTHER), 0U) << "the room of OTHER has not started";
}

TEST_F(ServerManagerTest, CrashOfAPlayerDoesNotStopTheGameOfTheOthers) {
    connect(CLIENT);
    connect(OTHER);
    const auto id = createRoom(CLIENT);
    message(OTHER, protocol::LobbyJoin{.roomId = id});
    message(CLIENT, protocol::LobbyReady{.ready = true});
    message(OTHER, protocol::LobbyReady{.ready = true});
    ASSERT_TRUE(waitForMessage<protocol::GameStart>(OTHER).has_value());

    push(EventType::Disconnected, OTHER);
    ASSERT_TRUE(waitForMessage<protocol::PlayerLeft>(CLIENT).has_value());
    const auto seen = countOf<protocol::WorldState>(CLIENT);

    ASSERT_TRUE(waitForMessage<protocol::WorldState>(CLIENT, seen + 1).has_value());
    const auto snapshot = waitForMessage<protocol::WorldState>(CLIENT, seen + 1);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->entities.size(), 1U) << "the ship of the player who left is gone";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
