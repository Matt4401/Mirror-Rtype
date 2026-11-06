#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/Engine.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/Entity.hpp"
#include "net/ClientConnection.hpp"
#include "net/INetworkClient.hpp"
#include "net/INetworkEventSource.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/PeerGroupConnection.hpp"

namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
using ecs::net::NetworkEvent;
using ecs::net::PeerId;

/** @brief Hands out the events pushed by the test, once each. */
class FakeSource final : public ecs::net::INetworkEventSource {
  public:
    std::vector<NetworkEvent> poll() override { return std::exchange(next, {}); }

    std::vector<NetworkEvent> next;
};

/** @brief Records every call instead of sending. */
class FakeService final : public ecs::net::INetworkService {
  public:
    struct Sent {
        PeerId peer;
        Bytes data;
        Channel channel;
    };

    void send(const PeerId peer, const Bytes& data, const Channel channel) override {
        sent.push_back({.peer = peer, .data = data, .channel = channel});
    }
    void broadcast(const Bytes& data, const Channel channel) override {
        sent.push_back({.peer = 0, .data = data, .channel = channel});
    }
    void kick(const PeerId peer) override { kicked.push_back(peer); }

    std::vector<Sent> sent;
    std::vector<PeerId> kicked;
};

/** @brief Client transport recording what it is asked to send. */
class FakeClient final : public ecs::net::INetworkClient {
  public:
    bool connect(const std::string& /*host*/, std::uint16_t /*port*/) override { return true; }
    [[nodiscard]] ecs::net::DisconnectReason disconnectReason() const override {
        return ecs::net::DisconnectReason::ConnectionLost;
    }
    bool openUdp(std::uint16_t /*port*/) override { return true; }
    void closeUdp() override {}
    void send(const Bytes& data, Channel /*channel*/) override { sent.push_back(data); }
    std::vector<NetworkEvent> poll() override { return std::exchange(next, {}); }
    void stop() override { running = false; }
    [[nodiscard]] bool isRunning() const override { return running; }

    std::vector<Bytes> sent;
    std::vector<NetworkEvent> next;
    bool running = true;
};

/** @brief Server transport recording what it is asked to send. */
class FakeServer final : public ecs::net::INetworkServer {
  public:
    bool start(std::uint16_t /*port*/) override { return true; }
    void send(const PeerId peer, const Bytes& /*data*/, Channel /*channel*/) override { sentTo.push_back(peer); }
    void broadcast(const Bytes& /*data*/, Channel /*channel*/) override { ++broadcasts; }
    void kick(const PeerId peer) override { kicked.push_back(peer); }
    std::vector<NetworkEvent> poll() override { return {}; }
    void stop() override {}
    [[nodiscard]] bool isRunning() const override { return true; }

    std::vector<PeerId> sentTo;
    std::vector<PeerId> kicked;
    int broadcasts = 0;
};

class NetworkPluginTest : public ::testing::Test {
  protected:
    void SetUp() override {
        engine.addPlugin<engine::CorePlugin>();
        engine.addPlugin<ecs::net::NetworkPlugin>(source, service);
        engine.init();
    }

    std::shared_ptr<FakeSource> source = std::make_shared<FakeSource>();
    std::shared_ptr<FakeService> service = std::make_shared<FakeService>();
    engine::Engine engine;
};

}  // namespace

TEST_F(NetworkPluginTest, PublishesTransportEventsOnTheEventBus) {
    std::vector<PeerId> connected;
    std::vector<PeerId> disconnected;
    std::vector<ecs::net::PacketReceived> packets;
    engine.eventBus().subscribe<ecs::net::PeerConnected>(
        [&](const ecs::net::PeerConnected& ev) { connected.push_back(ev.peer); });
    engine.eventBus().subscribe<ecs::net::PeerDisconnected>(
        [&](const ecs::net::PeerDisconnected& ev) { disconnected.push_back(ev.peer); });
    engine.eventBus().subscribe<ecs::net::PacketReceived>(
        [&](const ecs::net::PacketReceived& ev) { packets.push_back(ev); });

    source->next = {
        {.type = EventType::Connected, .peer = 3, .data = {}, .channel = Channel::Reliable},
        {.type = EventType::Data, .peer = 3, .data = {1, 2, 3}, .channel = Channel::Unreliable},
        {.type = EventType::Disconnected, .peer = 3, .data = {}, .channel = Channel::Reliable},
    };
    engine.advance();

    EXPECT_EQ(connected, std::vector<PeerId>{3});
    EXPECT_EQ(disconnected, std::vector<PeerId>{3});
    ASSERT_EQ(packets.size(), 1U);
    EXPECT_EQ(packets[0].peer, 3U);
    EXPECT_EQ(packets[0].channel, Channel::Unreliable);
    EXPECT_EQ(packets[0].data, (Bytes{1, 2, 3}));
}

TEST_F(NetworkPluginTest, KeepsTheArrivalOrder) {
    std::vector<int> order;
    engine.eventBus().subscribe<ecs::net::PeerConnected>([&](const auto&) { order.push_back(0); });
    engine.eventBus().subscribe<ecs::net::PacketReceived>([&](const auto&) { order.push_back(1); });
    engine.eventBus().subscribe<ecs::net::PeerDisconnected>([&](const auto&) { order.push_back(2); });

    source->next = {
        {.type = EventType::Data, .peer = 1, .data = {0}, .channel = Channel::Reliable},
        {.type = EventType::Disconnected, .peer = 1, .data = {}, .channel = Channel::Reliable},
        {.type = EventType::Connected, .peer = 2, .data = {}, .channel = Channel::Reliable},
    };
    engine.advance();

    EXPECT_EQ(order, (std::vector<int>{1, 2, 0}));
}

TEST_F(NetworkPluginTest, PublishesEachEventOnce) {
    int count = 0;
    engine.eventBus().subscribe<ecs::net::PeerConnected>([&](const auto&) { ++count; });

    source->next = {{.type = EventType::Connected, .peer = 1, .data = {}, .channel = Channel::Reliable}};
    engine.advance();
    engine.advance();

    EXPECT_EQ(count, 1);
}

TEST_F(NetworkPluginTest, RegistersTheServiceInTheMediator) {
    const auto found = engine.mediator().getService<ecs::net::INetworkService>();
    ASSERT_TRUE(found.has_value());

    found->get().send(4, {7}, Channel::Reliable);
    ASSERT_EQ(service->sent.size(), 1U);
    EXPECT_EQ(service->sent[0].peer, 4U);
    EXPECT_EQ(service->sent[0].data, Bytes{7});
}

TEST_F(NetworkPluginTest, ShutdownUnregistersTheService) {
    engine.shutdown();

    EXPECT_FALSE(engine.mediator().hasService<ecs::net::INetworkService>());
}

TEST_F(NetworkPluginTest, CreatesTheEntityMapResource) {
    EXPECT_TRUE(engine.registry().hasResource<ecs::net::NetworkEntityMap>());
}

TEST_F(NetworkPluginTest, KillingAnEntityReleasesItsNetworkId) {
    auto& map = engine.registry().getResource<ecs::net::NetworkEntityMap>();
    const ecs::Entity entity = engine.registry().spawnEntity();
    const auto id = map.assign(entity);
    ASSERT_EQ(map.entityOf(id), entity);

    engine.registry().killEntity(entity);

    EXPECT_FALSE(map.entityOf(id).has_value());
    EXPECT_EQ(map.size(), 0U);
}

TEST_F(NetworkPluginTest, ShutdownRemovesTheDestroyListener) {
    auto& map = engine.registry().getResource<ecs::net::NetworkEntityMap>();
    const ecs::Entity entity = engine.registry().spawnEntity();
    const auto id = map.assign(entity);

    engine.shutdown();
    engine.registry().killEntity(entity);

    EXPECT_EQ(map.entityOf(id), entity) << "the plugin no longer maintains the map after shutdown";
}

TEST(NetworkPlugin, ReusesAnEntityMapCreatedBeforeInit) {
    engine::Engine engine;
    auto& map = engine.registry().emplaceResource<ecs::net::NetworkEntityMap>();
    const ecs::Entity entity = engine.registry().spawnEntity();
    const auto id = map.assign(entity);

    engine.addPlugin<engine::CorePlugin>();
    engine.addPlugin<ecs::net::NetworkPlugin>(std::make_shared<FakeSource>(), std::make_shared<FakeService>());
    engine.init();

    EXPECT_EQ(&engine.registry().getResource<ecs::net::NetworkEntityMap>(), &map);
    EXPECT_EQ(map.entityOf(id), entity) << "existing bindings are kept";
    engine.registry().killEntity(entity);
    EXPECT_FALSE(map.entityOf(id).has_value()) << "the existing map is maintained by the plugin";
}

TEST(ClientConnection, OnlySendsToTheServer) {
    const auto client = std::make_shared<FakeClient>();
    ecs::net::ClientConnection connection(client);

    connection.send(ecs::net::SERVER_PEER, {1}, Channel::Reliable);
    connection.send(5, {2}, Channel::Reliable);
    connection.broadcast({3}, Channel::Unreliable);

    EXPECT_EQ(client->sent, (std::vector<Bytes>{{1}, {3}}));
}

TEST(ClientConnection, ForwardsPollAndKick) {
    const auto client = std::make_shared<FakeClient>();
    ecs::net::ClientConnection connection(client);
    client->next = {
        {.type = EventType::Connected, .peer = ecs::net::SERVER_PEER, .data = {}, .channel = Channel::Reliable}};

    EXPECT_EQ(connection.poll().size(), 1U);
    EXPECT_TRUE(connection.poll().empty());
    connection.kick(ecs::net::SERVER_PEER);
    EXPECT_FALSE(client->isRunning());
}

TEST(ClientConnection, FeedsTheNetworkPlugin) {
    const auto client = std::make_shared<FakeClient>();
    const auto connection = std::make_shared<ecs::net::ClientConnection>(client);
    engine::Engine engine;
    engine.addPlugin<engine::CorePlugin>();
    engine.addPlugin<ecs::net::NetworkPlugin>(connection, connection);
    int packets = 0;
    engine.eventBus().subscribe<ecs::net::PacketReceived>([&](const auto&) { ++packets; });

    client->next = {
        {.type = EventType::Data, .peer = ecs::net::SERVER_PEER, .data = {9}, .channel = Channel::Unreliable}};
    engine.advance();
    engine.mediator().getService<ecs::net::INetworkService>()->get().send(ecs::net::SERVER_PEER, {4},
                                                                          Channel::Reliable);

    EXPECT_EQ(packets, 1);
    EXPECT_EQ(client->sent, std::vector<Bytes>{{4}});
}

TEST(PeerGroupConnection, OnlyReachesThePeersOfTheGroup) {
    FakeServer server;
    ecs::net::PeerGroupConnection group(server);
    group.addPeer(1);
    group.addPeer(2);

    group.send(1, {7}, Channel::Reliable);
    group.send(3, {7}, Channel::Reliable);
    group.kick(3);
    group.kick(2);
    EXPECT_EQ(server.sentTo, std::vector<PeerId>{1});
    EXPECT_EQ(server.kicked, std::vector<PeerId>{2});

    server.sentTo.clear();
    group.broadcast({7}, Channel::Unreliable);
    std::ranges::sort(server.sentTo);
    EXPECT_EQ(server.sentTo, (std::vector<PeerId>{1, 2}));
    EXPECT_EQ(server.broadcasts, 0) << "the transport's broadcast would reach every group";
}

TEST(PeerGroupConnection, RemovedPeerIsNoLongerReachable) {
    FakeServer server;
    ecs::net::PeerGroupConnection group(server);
    group.addPeer(1);
    group.removePeer(1);

    group.send(1, {7}, Channel::Reliable);
    group.broadcast({7}, Channel::Reliable);
    group.push({.type = EventType::Data, .peer = 1, .data = {7}, .channel = Channel::Unreliable});

    EXPECT_TRUE(server.sentTo.empty());
    EXPECT_TRUE(group.poll().empty());
}

TEST(PeerGroupConnection, HandsOutTheEventsOfItsPeersOnce) {
    FakeServer server;
    ecs::net::PeerGroupConnection group(server);
    group.addPeer(1);

    group.push({.type = EventType::Data, .peer = 1, .data = {7}, .channel = Channel::Unreliable});
    group.push({.type = EventType::Data, .peer = 2, .data = {8}, .channel = Channel::Unreliable});

    const auto events = group.poll();
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].data, Bytes{7});
    EXPECT_TRUE(group.poll().empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
