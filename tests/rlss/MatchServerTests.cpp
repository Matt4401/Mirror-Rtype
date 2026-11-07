#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "MatchServer.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "protocol/Protocol.hpp"

namespace protocol = rlss::protocol;
using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
using ecs::net::NetworkEvent;
using ecs::net::PeerId;

namespace {

/** @brief In-memory transport: the test queues events, the server's sends are recorded. */
class FakeTransport final : public ecs::net::INetworkServer {
  public:
    /** @brief A message sent by the server. */
    struct Sent {
        PeerId peer = 0;
        protocol::Message message;
    };

    bool start(std::uint16_t /*port*/) override { return true; }
    void stop() override {}
    [[nodiscard]] bool isRunning() const override { return true; }
    std::vector<NetworkEvent> poll() override { return std::exchange(incoming, {}); }
    void send(const PeerId peer, const Bytes& data, const Channel /*channel*/) override {
        if (const auto message = protocol::decode(data)) {
            sent.push_back({.peer = peer, .message = *message});
        }
    }
    void broadcast(const Bytes& /*data*/, Channel /*channel*/) override {}
    void kick(PeerId /*peer*/) override {}

    std::vector<NetworkEvent> incoming;
    std::vector<Sent> sent;
};

/** @brief A `MatchServer` running its matches inline (not threaded), with the Wren scripts of the repository. */
class MatchServerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        std::filesystem::current_path(RLSS_PROJECT_DIR);
        transport = std::make_shared<FakeTransport>();
        server.emplace(transport, false);
    }

    void connect(const PeerId peer) {
        transport->incoming.push_back({.type = EventType::Connected, .peer = peer, .data = {}, .channel = {}});
        server->update();
    }

    void disconnect(const PeerId peer) {
        transport->incoming.push_back({.type = EventType::Disconnected, .peer = peer, .data = {}, .channel = {}});
        server->update();
    }

    void receive(const PeerId peer, const Bytes& data) {
        transport->incoming.push_back(
            {.type = EventType::Data, .peer = peer, .data = data, .channel = Channel::Reliable});
        server->update();
    }

    /** @brief Last message of type `T` sent to a peer, if any. */
    template <typename T>
    std::optional<T> lastTo(const PeerId peer) const {
        std::optional<T> found;
        for (const auto& sent : transport->sent) {
            if (sent.peer == peer && std::holds_alternative<T>(sent.message)) {
                found = std::get<T>(sent.message);
            }
        }
        return found;
    }

    /** @brief The answer to the last create / join request of a peer. */
    [[nodiscard]] protocol::JoinResult resultOf(const PeerId peer) const {
        const auto result = lastTo<protocol::JoinResult>(peer);
        EXPECT_TRUE(result.has_value());
        return result.value_or(protocol::JoinResult{.status = protocol::JoinStatus::NotFound});
    }

    std::shared_ptr<FakeTransport> transport;
    std::optional<rlss::server::MatchServer> server;
};

}  // namespace

TEST_F(MatchServerTest, AConnectedPlayerGetsTheMatchList) {
    connect(1);
    const auto list = lastTo<protocol::MatchList>(1);
    ASSERT_TRUE(list.has_value());
    EXPECT_EQ(list->online, 1);
    EXPECT_TRUE(list->matches.empty());
}

TEST_F(MatchServerTest, CreatingAMatchPutsThePlayerInItsBlueTeam) {
    connect(1);
    receive(1, protocol::encode(protocol::CreateMatch{}));
    const protocol::JoinResult result = resultOf(1);
    EXPECT_EQ(result.status, protocol::JoinStatus::Joined);
    EXPECT_NE(result.matchId, 0);
    EXPECT_EQ(server->matchOf(1), result.matchId);
    EXPECT_EQ(server->matchCount(), 1U);

    server->update();  // the match spawns the car on its next frame
    const auto welcome = lastTo<protocol::Welcome>(1);
    ASSERT_TRUE(welcome.has_value());
    EXPECT_EQ(welcome->team, protocol::Team::Blue);
    const auto status = lastTo<protocol::MatchStatus>(1);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->phase, protocol::MatchPhase::Kickoff);  // a new match starts with a kickoff

    const protocol::MatchList list = server->matchList();
    ASSERT_EQ(list.matches.size(), 1U);
    EXPECT_EQ(list.matches[0].id, result.matchId);
    EXPECT_EQ(list.matches[0].bluePlayers, 1);
    EXPECT_EQ(list.matches[0].orangePlayers, 0);
    EXPECT_EQ(list.matches[0].clock, 180);
}

TEST_F(MatchServerTest, PlayersFillTheTeamsTwoAgainstTwoThenTheMatchIsFull) {
    connect(1);
    receive(1, protocol::encode(protocol::CreateMatch{}));
    const std::uint16_t match = resultOf(1).matchId;
    const std::vector<protocol::Team> expected{protocol::Team::Orange, protocol::Team::Blue, protocol::Team::Orange};
    for (PeerId peer = 2; peer <= 4; ++peer) {
        connect(peer);
        receive(peer, protocol::encode(protocol::JoinMatch{.matchId = match}));
        EXPECT_EQ(resultOf(peer).status, protocol::JoinStatus::Joined);
        server->update();
        const auto welcome = lastTo<protocol::Welcome>(peer);
        ASSERT_TRUE(welcome.has_value());
        EXPECT_EQ(welcome->team, expected.at(peer - 2));
    }
    const protocol::MatchInfo info = server->matchList().matches.at(0);
    EXPECT_EQ(info.bluePlayers, 2);
    EXPECT_EQ(info.orangePlayers, 2);

    connect(5);
    receive(5, protocol::encode(protocol::JoinMatch{.matchId = match}));
    EXPECT_EQ(resultOf(5).status, protocol::JoinStatus::Full);
    EXPECT_FALSE(server->matchOf(5).has_value());
}

TEST_F(MatchServerTest, ALeavingPlayerFreesTheirSlotForTheirTeam) {
    connect(1);
    receive(1, protocol::encode(protocol::CreateMatch{}));
    const std::uint16_t match = resultOf(1).matchId;
    connect(2);
    receive(2, protocol::encode(protocol::JoinMatch{.matchId = match}));
    connect(3);
    receive(3, protocol::encode(protocol::JoinMatch{.matchId = match}));
    receive(2, protocol::encode(protocol::LeaveMatch{}));  // orange leaves: 2 blue, 0 orange
    EXPECT_FALSE(server->matchOf(2).has_value());
    EXPECT_TRUE(lastTo<protocol::MatchList>(2).has_value());  // back in the menu

    connect(4);
    receive(4, protocol::encode(protocol::JoinMatch{.matchId = match}));
    server->update();
    EXPECT_EQ(lastTo<protocol::Welcome>(4)->team, protocol::Team::Orange);
}

TEST_F(MatchServerTest, TheLastPlayerLeavingClosesTheMatch) {
    connect(1);
    connect(2);
    receive(1, protocol::encode(protocol::CreateMatch{}));
    const std::uint16_t match = resultOf(1).matchId;
    receive(2, protocol::encode(protocol::JoinMatch{.matchId = match}));
    receive(1, protocol::encode(protocol::LeaveMatch{}));
    EXPECT_EQ(server->matchCount(), 1U);
    disconnect(2);  // a crash counts as leaving
    EXPECT_EQ(server->matchCount(), 0U);
    EXPECT_TRUE(server->matchList().matches.empty());

    receive(1, protocol::encode(protocol::JoinMatch{.matchId = match}));
    EXPECT_EQ(resultOf(1).status, protocol::JoinStatus::NotFound);
}

TEST_F(MatchServerTest, MatchesAreIsolatedFromEachOther) {
    connect(1);
    connect(2);
    receive(1, protocol::encode(protocol::CreateMatch{}));
    receive(2, protocol::encode(protocol::CreateMatch{}));
    const std::uint16_t first = resultOf(1).matchId;
    const std::uint16_t second = resultOf(2).matchId;
    EXPECT_NE(first, second);
    server->update();
    // Each player is alone in a match: both play blue, and each only gets the snapshots of its own match.
    EXPECT_EQ(lastTo<protocol::Welcome>(1)->team, protocol::Team::Blue);
    EXPECT_EQ(lastTo<protocol::Welcome>(2)->team, protocol::Team::Blue);
    transport->sent.clear();
    // Snapshots follow the simulation steps, which follow the real clock of each match.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!(lastTo<protocol::Snapshot>(1) && lastTo<protocol::Snapshot>(2)) &&
           std::chrono::steady_clock::now() < deadline) {
        server->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    int snapshots = 0;
    for (const auto& sent : transport->sent) {
        if (const auto* snapshot = std::get_if<protocol::Snapshot>(&sent.message)) {
            ++snapshots;
            EXPECT_EQ(snapshot->matchId, sent.peer == 1 ? first : second);
            EXPECT_EQ(snapshot->entities.size(), 2U);  // its car and its ball
        }
    }
    EXPECT_GE(snapshots, 2);
}

TEST_F(MatchServerTest, TheServerHostsALimitedNumberOfMatches) {
    for (PeerId peer = 1; peer <= protocol::MAX_MATCHES; ++peer) {
        connect(peer);
        receive(peer, protocol::encode(protocol::CreateMatch{}));
        ASSERT_EQ(resultOf(peer).status, protocol::JoinStatus::Joined);
    }
    const PeerId late = protocol::MAX_MATCHES + 1;
    connect(late);
    receive(late, protocol::encode(protocol::CreateMatch{}));
    EXPECT_EQ(resultOf(late).status, protocol::JoinStatus::ServerFull);
    EXPECT_EQ(server->matchCount(), protocol::MAX_MATCHES);
}

TEST_F(MatchServerTest, MalformedOrOutOfPlaceRequestsAreIgnored) {
    connect(1);
    receive(1, Bytes{7, 0});                                             // truncated join
    receive(1, Bytes{6, 0, 0});                                          // create with trailing bytes
    receive(1, protocol::encode(protocol::LeaveMatch{}));                // not in a match
    receive(1, protocol::encode(protocol::PlayerInput{.sequence = 1}));  // input from the menu
    receive(9, protocol::encode(protocol::CreateMatch{}));               // never connected
    EXPECT_EQ(server->matchCount(), 0U);
    EXPECT_FALSE(lastTo<protocol::JoinResult>(1).has_value());

    receive(1, protocol::encode(protocol::CreateMatch{}));
    receive(1, protocol::encode(protocol::CreateMatch{}));  // already playing
    EXPECT_EQ(server->matchCount(), 1U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
