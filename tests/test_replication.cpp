#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "components/InputBuffer.hpp"
#include "components/Interpolated.hpp"
#include "components/NetworkId.hpp"
#include "components/NetworkOwner.hpp"
#include "components/Predicted.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkEventSource.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/IReplicationCodec.hpp"
#include "net/NetworkConfig.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/ReplicationEvents.hpp"
#include "net/ReplicationSystems.hpp"
#include "net/ServerReplicationPlugin.hpp"
#include "net/SnapshotApplier.hpp"

namespace {

using ecs::NetInput;
using ecs::net::Bytes;
using ecs::net::Channel;
using ecs::net::EventType;
using ecs::net::NetworkEvent;
using ecs::net::PeerId;

// A power of two: exact in binary, so that adding it to the fake clock makes exactly one step due.
constexpr float DT = 1.0F / 64.0F;
constexpr std::uint8_t SNAPSHOT_TAG = 'S';
constexpr std::uint8_t INPUT_TAG = 'I';

/** @brief Appends the raw bytes of a trivially copyable value. */
template <typename T>
void put(Bytes& out, const T& value) {
    const auto* raw =
        reinterpret_cast<const std::uint8_t*>(&value);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    out.insert(out.end(), raw, raw + sizeof(T));        // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

/** @brief Reads a value at `offset` if it fits, advancing the offset. */
template <typename T>
bool get(const Bytes& in, std::size_t& offset, T& value) {
    if (in.size() < offset || in.size() - offset < sizeof(T)) {
        return false;
    }
    std::memcpy(&value, in.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

/**
 * @brief Smallest codec a game could write, without protobuf nor any R-Type type: it proves that the replication only
 * depends on the engine.
 *
 * Snapshot: `['S'][tick u32][last u32][part u8][parts u8][count u16]` then `count` times
 * `[id u32][kind u32][x f32][y f32]`.
 * Input: `['I'][sequence u32][held count u8]` then `count` times `[action u32]` (axes are not encoded).
 */
class TestCodec final : public ecs::net::IReplicationCodec {
  public:
    struct Entry {
        std::uint32_t id;
        std::uint32_t kind;
        float x;
        float y;
    };

    void beginSnapshot(const std::uint32_t tick) override {
        _tick = tick;
        _written.clear();
    }

    void writeEntity(const ecs::Registry& registry, const ecs::Entity entity, const std::uint32_t id,
                     const ecs::Replicated& replicated) override {
        ecs::Transform transform;
        if (registry.hasComponent<ecs::Transform>(entity)) {
            transform = registry.getComponents<ecs::Transform>().get(entity);
        }
        _written.push_back({.id = id, .kind = replicated.kind, .x = transform.x, .y = transform.y});
    }

    std::vector<Bytes> encodeSnapshot(const std::uint32_t lastProcessedInput) override {
        const std::size_t perPart = entitiesPerPart == 0 ? std::max<std::size_t>(_written.size(), 1) : entitiesPerPart;
        const std::size_t partCount = std::max<std::size_t>((_written.size() + perPart - 1) / perPart, 1);
        std::vector<Bytes> parts;
        for (std::size_t part = 0; part < partCount; ++part) {
            const std::size_t first = part * perPart;
            const std::size_t last = std::min(first + perPart, _written.size());
            Bytes out{SNAPSHOT_TAG};
            put(out, _tick);
            put(out, lastProcessedInput);
            put(out, static_cast<std::uint8_t>(part));
            put(out, static_cast<std::uint8_t>(partCount));
            put(out, static_cast<std::uint16_t>(last - first));
            for (std::size_t i = first; i < last; ++i) {
                const Entry& entry = _written.at(i);
                put(out, entry.id);
                put(out, entry.kind);
                put(out, entry.x);
                put(out, entry.y);
            }
            out.resize(out.size() + padding, 0);
            parts.push_back(std::move(out));
        }
        return parts;
    }

    std::optional<NetInput> decodeInput(const Bytes& data) override {
        std::size_t offset = 1;
        NetInput input;
        std::uint8_t count = 0;
        if (data.empty() || data[0] != INPUT_TAG || !get(data, offset, input.sequence) || !get(data, offset, count)) {
            return std::nullopt;
        }
        input.held.resize(count);
        for (std::uint32_t& action : input.held) {
            if (!get(data, offset, action)) {
                return std::nullopt;
            }
        }
        return input;
    }

    Bytes encodeInput(const NetInput& input) override {
        Bytes out{INPUT_TAG};
        put(out, input.sequence);
        put(out, static_cast<std::uint8_t>(input.held.size()));
        for (const std::uint32_t action : input.held) {
            put(out, action);
        }
        return out;
    }

    std::optional<ecs::net::SnapshotHeader> decodeSnapshot(const Bytes& data) override {
        std::size_t offset = 1;
        ecs::net::SnapshotHeader header;
        std::uint8_t part = 0;
        std::uint8_t parts = 0;
        std::uint16_t count = 0;
        if (data.empty() || data[0] != SNAPSHOT_TAG || !get(data, offset, header.tick) ||
            !get(data, offset, header.lastProcessedInput) || !get(data, offset, part) || !get(data, offset, parts) ||
            !get(data, offset, count)) {
            return std::nullopt;
        }
        header.partIndex = part;
        header.partCount = parts;
        _read.clear();
        for (std::uint16_t i = 0; i < count; ++i) {
            Entry entry{};
            if (!get(data, offset, entry.id) || !get(data, offset, entry.kind) || !get(data, offset, entry.x) ||
                !get(data, offset, entry.y)) {
                _read.clear();
                return std::nullopt;
            }
            _read.push_back(entry);
        }
        return header;
    }

    void applySnapshot(ecs::net::SnapshotApplier& applier) override {
        for (const Entry& entry : _read) {
            if (const auto entity = applier.entity(entry.id, entry.kind)) {
                applier.setTransform(*entity, ecs::Transform{.x = entry.x, .y = entry.y});
            }
        }
    }

    /** @brief Zero bytes appended to each snapshot part, to test the size limit. */
    std::size_t padding = 0;
    /** @brief Largest number of entities in one part; 0 keeps them all in one part. */
    std::size_t entitiesPerPart = 0;

  private:
    std::uint32_t _tick = 0;
    std::vector<Entry> _written;
    std::vector<Entry> _read;
};

class FakeSource final : public ecs::net::INetworkEventSource {
  public:
    std::vector<NetworkEvent> poll() override { return std::exchange(next, {}); }

    void data(const PeerId peer, Bytes bytes) {
        next.push_back(
            {.type = EventType::Data, .peer = peer, .data = std::move(bytes), .channel = Channel::Unreliable});
    }

    std::vector<NetworkEvent> next;
};

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
    void kick(PeerId /*peer*/) override {}

    std::vector<Sent> sent;
};

/** @brief One engine with the network and one replication plugin, wired to fakes. */
template <typename ReplicationPlugin>
struct Side {
    Side() {
        engine.setFixedDeltaTime(DT);
        engine.addPlugin<engine::CorePlugin>();
        engine.addPlugin<ecs::net::NetworkPlugin>(source, service);
        engine.addPlugin<ReplicationPlugin>(codec);
        engine.init();
    }

    ecs::Registry& registry() { return engine.registry(); }

    /** @brief Lets one fixed step of time elapse, and runs the engine. */
    void step() {
        now += static_cast<double>(DT);
        engine.advance();
    }

    double now = 0.0;
    std::shared_ptr<FakeSource> source = std::make_shared<FakeSource>();
    std::shared_ptr<FakeService> service = std::make_shared<FakeService>();
    std::shared_ptr<TestCodec> codec = std::make_shared<TestCodec>();
    engine::Engine engine{[this]() -> double { return now; }};
};

using Server = Side<ecs::net::ServerReplicationPlugin>;
using Client = Side<ecs::net::ClientReplicationPlugin>;

/** @brief Spawns a player entity on the server, owned by `peer`. */
ecs::Entity spawnPlayer(ecs::Registry& registry, const PeerId peer, const float x = 0.0F) {
    const ecs::Entity entity = registry.spawnEntity();
    ecs::net::replicate(registry, entity, 1);
    registry.addComponent(entity, ecs::NetworkOwner{peer});
    registry.addComponent(entity, ecs::InputBuffer{});
    registry.addComponent(entity, ecs::Transform{.x = x});
    return entity;
}

/** @brief Delivers what the server sent to `peer` to the client, as received packets. */
void deliver(Server& server, Client& client, const PeerId peer) {
    for (auto& sent : server.service->sent) {
        if (sent.peer == peer) {
            client.source->data(ecs::net::SERVER_PEER, std::move(sent.data));
        }
    }
    server.service->sent.clear();
}

std::optional<ecs::Entity> clientEntity(Client& client, const std::uint32_t id) {
    return client.registry().getResource<ecs::net::NetworkEntityMap>().entityOf(id);
}

}  // namespace

// --- Server ----------------------------------------------------------------------------------------------------------

TEST(Replicate, GivesANetworkIdAndAKind) {
    ecs::Registry registry;
    const ecs::Entity entity = registry.spawnEntity();

    const std::uint32_t id = ecs::net::replicate(registry, entity, 7);

    EXPECT_NE(id, 0U);
    EXPECT_EQ(registry.getComponents<ecs::NetworkId>().get(entity).value, id);
    EXPECT_EQ(registry.getComponents<ecs::Replicated>().get(entity).kind, 7U);
    EXPECT_EQ(registry.getResource<ecs::net::NetworkEntityMap>().entityOf(id), entity);
}

TEST(ReceiveInput, QueuesTheInputOfTheOwnedEntityOnly) {
    ecs::Registry registry;
    const ecs::Entity mine = spawnPlayer(registry, 3);
    const ecs::Entity other = spawnPlayer(registry, 4);

    EXPECT_TRUE(ecs::net::receiveInput(registry, 3, {.sequence = 1, .held = {2}, .axes = {}}));
    EXPECT_FALSE(ecs::net::receiveInput(registry, 9, {.sequence = 1, .held = {2}, .axes = {}}))
        << "peer without entity";

    EXPECT_EQ(registry.getComponents<ecs::InputBuffer>().get(mine).pending.size(), 1U);
    EXPECT_TRUE(registry.getComponents<ecs::InputBuffer>().get(other).pending.empty());
}

TEST(ReceiveInput, QueuesTheInputOfEveryEntityOfThePeer) {
    ecs::Registry registry;
    const ecs::Entity first = spawnPlayer(registry, 3);
    const ecs::Entity second = spawnPlayer(registry, 3);

    EXPECT_TRUE(ecs::net::receiveInput(registry, 3, {.sequence = 1, .held = {2}, .axes = {}}));

    EXPECT_EQ(registry.getComponents<ecs::InputBuffer>().get(first).pending.size(), 1U);
    EXPECT_EQ(registry.getComponents<ecs::InputBuffer>().get(second).pending.size(), 1U);
}

TEST(ReceiveInput, ReportsAnInputEveryEntityDropped) {
    ecs::Registry registry;
    spawnPlayer(registry, 3);
    const NetInput input{.sequence = 1, .held = {}, .axes = {}};

    EXPECT_TRUE(ecs::net::receiveInput(registry, 3, input));
    EXPECT_FALSE(ecs::net::receiveInput(registry, 3, input)) << "duplicated";
}

TEST(ServerReplication, AppliesOneReceivedInputPerStep) {
    Server server;
    const ecs::Entity player = spawnPlayer(server.registry(), 3);
    std::vector<ecs::net::InputApplied> applied;
    server.engine.eventBus().subscribe<ecs::net::InputApplied>(
        [&](const ecs::net::InputApplied& ev) { applied.push_back(ev); });

    server.source->data(3, server.codec->encodeInput({.sequence = 1, .held = {5}, .axes = {}}));
    server.source->data(3, server.codec->encodeInput({.sequence = 2, .held = {6}, .axes = {}}));
    server.source->data(3, {0xFF, 0x00});  // garbage: ignored
    server.step();

    ASSERT_EQ(applied.size(), 1U);
    EXPECT_EQ(applied[0].entity, player);
    EXPECT_EQ(applied[0].input.held, std::vector<std::uint32_t>{5});
    EXPECT_FALSE(applied[0].replayed);
    EXPECT_FLOAT_EQ(applied[0].deltaTime, server.engine.fixedDeltaTime());

    server.step();
    ASSERT_EQ(applied.size(), 2U);
    EXPECT_EQ(applied[1].input.sequence, 2U);
}

TEST(ServerReplication, SendsOneSnapshotPerOwnerAfterTheSteps) {
    Server server;
    spawnPlayer(server.registry(), 3);
    spawnPlayer(server.registry(), 4);
    const ecs::Entity enemy = server.registry().spawnEntity();
    ecs::net::replicate(server.registry(), enemy, 2);

    server.source->data(3, server.codec->encodeInput({.sequence = 10, .held = {}, .axes = {}}));
    server.step();

    ASSERT_EQ(server.service->sent.size(), 2U) << "one per peer owning an entity, none for the enemy";
    for (const auto& sent : server.service->sent) {
        EXPECT_EQ(sent.channel, Channel::Unreliable);
        const auto header = server.codec->decodeSnapshot(sent.data);
        ASSERT_TRUE(header.has_value());
        EXPECT_EQ(header->lastProcessedInput, sent.peer == 3 ? 10U : 0U);
    }
}

TEST(ServerReplication, SendsNothingWhenNoStepRan) {
    double now = 0.0;
    engine::Engine engine([&] { return now; });
    engine.setFixedDeltaTime(DT);
    auto service = std::make_shared<FakeService>();
    engine.addPlugin<engine::CorePlugin>();
    engine.addPlugin<ecs::net::NetworkPlugin>(std::make_shared<FakeSource>(), service);
    engine.addPlugin<ecs::net::ServerReplicationPlugin>(std::make_shared<TestCodec>());
    engine.init();
    spawnPlayer(engine.registry(), 3);

    engine.advance();
    EXPECT_TRUE(service->sent.empty());

    now += 3.5 * DT;
    EXPECT_EQ(engine.advance(), 3U);
    EXPECT_EQ(service->sent.size(), 1U) << "catch-up steps send a single snapshot";
}

TEST(ServerReplication, DropsSnapshotsTooLargeForADatagram) {
    Server server;
    spawnPlayer(server.registry(), 3);
    server.codec->padding = ecs::net::MAX_UNRELIABLE_PAYLOAD;

    server.step();

    EXPECT_TRUE(server.service->sent.empty());
}

TEST(ServerReplication, SendsEachPartOfASplitSnapshot) {
    Server server;
    spawnPlayer(server.registry(), 3);
    for (int i = 0; i < 4; ++i) {
        ecs::net::replicate(server.registry(), server.registry().spawnEntity(), 2);
    }
    server.codec->entitiesPerPart = 2;

    server.step();

    ASSERT_EQ(server.service->sent.size(), 3U) << "5 entities, 2 per part";
    for (std::uint32_t part = 0; part < 3; ++part) {
        const auto header = server.codec->decodeSnapshot(server.service->sent.at(part).data);
        ASSERT_TRUE(header.has_value());
        EXPECT_EQ(header->partIndex, part);
        EXPECT_EQ(header->partCount, 3U);
    }
}

TEST(ServerReplication, ResentInputsAreAppliedOnce) {
    Server server;
    spawnPlayer(server.registry(), 3);
    std::vector<std::uint32_t> applied;
    server.engine.eventBus().subscribe<ecs::net::InputApplied>(
        [&](const ecs::net::InputApplied& ev) { applied.push_back(ev.input.sequence); });
    const Bytes first = server.codec->encodeInput({.sequence = 1, .held = {}, .axes = {}});
    const Bytes second = server.codec->encodeInput({.sequence = 2, .held = {}, .axes = {}});

    server.source->data(3, first);
    server.step();
    server.source->data(3, first);
    server.source->data(3, second);
    server.step();
    server.source->data(3, first);
    server.source->data(3, second);
    server.step();

    EXPECT_EQ(applied, (std::vector<std::uint32_t>{1, 2}));
}

// --- Client ----------------------------------------------------------------------------------------------------------

TEST(ClientReplication, CreatesUpdatesAndRemovesEntities) {
    Server server;
    Client client;
    std::vector<ecs::net::ReplicatedEntitySpawned> spawned;
    std::vector<ecs::net::ReplicatedEntityDespawned> despawned;
    client.engine.eventBus().subscribe<ecs::net::ReplicatedEntitySpawned>(
        [&](const auto& ev) { spawned.push_back(ev); });
    client.engine.eventBus().subscribe<ecs::net::ReplicatedEntityDespawned>(
        [&](const auto& ev) { despawned.push_back(ev); });

    const ecs::Entity player = spawnPlayer(server.registry(), 3, 10.0F);
    const ecs::Entity enemy = server.registry().spawnEntity();
    const std::uint32_t enemyId = ecs::net::replicate(server.registry(), enemy, 2);
    server.registry().addComponent(enemy, ecs::Transform{.x = 50.0F, .y = 20.0F});
    const std::uint32_t playerId = server.registry().getComponents<ecs::NetworkId>().get(player).value;

    server.step();
    deliver(server, client, 3);
    client.step();

    ASSERT_EQ(spawned.size(), 2U);
    const auto local = clientEntity(client, enemyId);
    ASSERT_TRUE(local.has_value());
    EXPECT_EQ(client.registry().getComponents<ecs::Replicated>().get(*local).kind, 2U);
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(*local).x, 50.0F);
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(*local).y, 20.0F);

    server.registry().getComponents<ecs::Transform>().get(enemy).x = 40.0F;
    server.step();
    deliver(server, client, 3);
    client.step();
    EXPECT_EQ(spawned.size(), 2U) << "known entities are not created again";
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(*local).x, 40.0F);

    server.registry().killEntity(enemy);
    server.step();
    deliver(server, client, 3);
    client.step();
    ASSERT_EQ(despawned.size(), 1U);
    EXPECT_EQ(despawned[0].id, enemyId);
    EXPECT_FALSE(client.registry().isAlive(*local));
    EXPECT_FALSE(clientEntity(client, enemyId).has_value());
    EXPECT_TRUE(clientEntity(client, playerId).has_value());
}

TEST(ClientReplication, DropsStaleAndMalformedSnapshots) {
    Server server;
    Client client;
    const ecs::Entity player = spawnPlayer(server.registry(), 3, 1.0F);

    server.step();
    const Bytes older = server.service->sent.at(0).data;
    server.service->sent.clear();
    server.registry().getComponents<ecs::Transform>().get(player).x = 2.0F;
    server.step();
    deliver(server, client, 3);
    client.source->data(ecs::net::SERVER_PEER, older);
    Bytes truncated = older;
    truncated.resize(truncated.size() - 1);
    client.source->data(ecs::net::SERVER_PEER, truncated);
    client.source->data(ecs::net::SERVER_PEER, {});
    client.step();

    const auto local = clientEntity(client, 1);
    ASSERT_TRUE(local.has_value());
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(*local).x, 2.0F)
        << "the older snapshot arrived last but was dropped";
    EXPECT_EQ(client.registry().getResource<ecs::net::SnapshotState>().tick, 2U);
}

/** @brief A server whose snapshots are split into one part per entity, and a client. */
struct SplitSnapshot : public ::testing::Test {
    void SetUp() override {
        spawnPlayer(server.registry(), 3);
        server.codec->entitiesPerPart = 1;
        client.engine.eventBus().subscribe<ecs::net::ReplicatedEntityDespawned>(
            [this](const auto& ev) { despawned.push_back(ev.id); });
    }

    /** @brief Runs the server once and takes what it sent, one payload per part. */
    std::vector<Bytes> nextSnapshot() {
        server.step();
        std::vector<Bytes> parts;
        for (auto& sent : server.service->sent) {
            parts.push_back(std::move(sent.data));
        }
        server.service->sent.clear();
        return parts;
    }

    /** @brief Hands packets to the client and runs it. */
    void receive(const std::vector<Bytes>& parts) {
        for (const Bytes& part : parts) {
            client.source->data(ecs::net::SERVER_PEER, part);
        }
        client.engine.advance();
    }

    Server server;
    Client client;
    std::vector<std::uint32_t> despawned;
};

TEST_F(SplitSnapshot, AppliesEveryPart) {
    const ecs::Entity enemy = server.registry().spawnEntity();
    const std::uint32_t id = ecs::net::replicate(server.registry(), enemy, 2);
    server.registry().addComponent(enemy, ecs::Transform{.x = 7.0F});

    const auto parts = nextSnapshot();
    ASSERT_EQ(parts.size(), 2U);
    receive(parts);

    const auto local = clientEntity(client, id);
    ASSERT_TRUE(local.has_value());
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(*local).x, 7.0F);
    EXPECT_TRUE(despawned.empty());
}

TEST_F(SplitSnapshot, LostPartDoesNotRemoveTheEntitiesItCarried) {
    const ecs::Entity enemy = server.registry().spawnEntity();
    const std::uint32_t id = ecs::net::replicate(server.registry(), enemy, 2);
    receive(nextSnapshot());
    ASSERT_TRUE(clientEntity(client, id).has_value());

    const auto parts = nextSnapshot();
    ASSERT_EQ(parts.size(), 2U);
    receive({parts.front()});

    EXPECT_TRUE(despawned.empty());
    EXPECT_TRUE(clientEntity(client, id).has_value());
}

TEST_F(SplitSnapshot, RemovesAnEntityOnceASnapshotIsComplete) {
    const ecs::Entity enemy = server.registry().spawnEntity();
    const std::uint32_t id = ecs::net::replicate(server.registry(), enemy, 2);
    const ecs::Entity other = server.registry().spawnEntity();
    const std::uint32_t otherId = ecs::net::replicate(server.registry(), other, 2);
    receive(nextSnapshot());

    server.registry().killEntity(enemy);
    const auto parts = nextSnapshot();
    ASSERT_EQ(parts.size(), 2U);
    receive({parts.back()});
    EXPECT_TRUE(despawned.empty()) << "the first part may carry the entity";
    receive({parts.front()});

    EXPECT_EQ(despawned, std::vector<std::uint32_t>{id});
    EXPECT_TRUE(clientEntity(client, otherId).has_value());
}

TEST_F(SplitSnapshot, IgnoresAPartReceivedTwice) {
    const ecs::Entity serverShip = server.registry().spawnEntity();
    ecs::net::replicate(server.registry(), serverShip, 2);
    client.engine.eventBus().subscribe<ecs::net::ReplicatedEntitySpawned>(
        [this](const auto& ev) { client.registry().addComponent(ev.entity, ecs::Predicted{}); });
    receive(nextSnapshot());
    client.engine.eventBus().publish(ecs::net::LocalInput{.input = {.sequence = 1, .held = {}, .axes = {}}});
    int replayed = 0;
    client.engine.eventBus().subscribe<ecs::net::InputApplied>([&replayed](const auto& ev) {
        if (ev.replayed) {
            ++replayed;
        }
    });

    const auto parts = nextSnapshot();
    ASSERT_EQ(parts.size(), 2U);
    receive({parts.front(), parts.front(), parts.back(), parts.back()});

    EXPECT_EQ(replayed, 2) << "one replay per predicted entity: the duplicates change nothing";
}

TEST_F(SplitSnapshot, DropsAPartOfAnOlderSnapshot) {
    const ecs::Entity enemy = server.registry().spawnEntity();
    const std::uint32_t id = ecs::net::replicate(server.registry(), enemy, 2);
    server.registry().addComponent(enemy, ecs::Transform{.x = 1.0F});
    const auto older = nextSnapshot();
    server.registry().getComponents<ecs::Transform>().get(enemy).x = 2.0F;
    receive(nextSnapshot());

    receive(older);

    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(*clientEntity(client, id)).x, 2.0F);
}

TEST(ClientReplication, DropsPartsWithAnInvalidHeader) {
    /** @brief Codec returning a fixed header, as a malicious server could announce it. */
    class ForgedCodec final : public ecs::net::IReplicationCodec {
      public:
        explicit ForgedCodec(const ecs::net::SnapshotHeader& forged) : _forged(forged) {}
        void beginSnapshot(std::uint32_t /*tick*/) override {}
        void writeEntity(const ecs::Registry& /*registry*/, ecs::Entity /*entity*/, std::uint32_t /*id*/,
                         const ecs::Replicated& /*replicated*/) override {}
        std::vector<Bytes> encodeSnapshot(std::uint32_t /*lastProcessedInput*/) override { return {}; }
        std::optional<NetInput> decodeInput(const Bytes& /*data*/) override { return std::nullopt; }
        Bytes encodeInput(const NetInput& /*input*/) override { return {}; }
        std::optional<ecs::net::SnapshotHeader> decodeSnapshot(const Bytes& /*data*/) override { return _forged; }
        void applySnapshot(ecs::net::SnapshotApplier& /*applier*/) override { ++applied; }
        int applied = 0;

      private:
        ecs::net::SnapshotHeader _forged;
    };

    const std::vector<ecs::net::SnapshotHeader> invalid{
        {.tick = 1, .lastProcessedInput = 0, .partIndex = 0, .partCount = 0},
        {.tick = 1, .lastProcessedInput = 0, .partIndex = 2, .partCount = 2},
        {.tick = 1, .lastProcessedInput = 0, .partIndex = 0, .partCount = ecs::net::MAX_SNAPSHOT_PARTS + 1},
    };
    for (const auto& header : invalid) {
        const auto codec = std::make_shared<ForgedCodec>(header);
        const auto source = std::make_shared<FakeSource>();
        engine::Engine engine;
        engine.addPlugin<engine::CorePlugin>();
        engine.addPlugin<ecs::net::NetworkPlugin>(source, std::make_shared<FakeService>());
        engine.addPlugin<ecs::net::ClientReplicationPlugin>(codec);
        engine.init();

        source->data(ecs::net::SERVER_PEER, {1});
        engine.advance();

        EXPECT_EQ(codec->applied, 0);
    }
}

TEST(ClientReplication, IgnoresAnInvalidNetworkId) {
    ecs::Registry registry;
    ecs::net::NetworkEntityMap map;
    ecs::net::SnapshotApplier applier(registry, map, DT);

    EXPECT_FALSE(applier.entity(0, 1).has_value());
    EXPECT_TRUE(applier.spawned().empty());
}

TEST(ClientReplication, InterpolatesBetweenSnapshots) {
    Server server;
    Client client;
    const ecs::Entity enemy = server.registry().spawnEntity();
    const std::uint32_t id = ecs::net::replicate(server.registry(), enemy, 2);
    server.registry().addComponent(enemy, ecs::Transform{.x = 0.0F});
    spawnPlayer(server.registry(), 3);
    client.engine.eventBus().subscribe<ecs::net::ReplicatedEntitySpawned>(
        [&](const auto& ev) { client.registry().addComponent(ev.entity, ecs::Interpolated{}); });

    server.step();
    deliver(server, client, 3);
    client.engine.advance();
    const ecs::Entity local = *clientEntity(client, id);
    // The component is added after the first snapshot was applied: the second one initializes it.
    server.registry().getComponents<ecs::Transform>().get(enemy).x = 6.0F;
    server.step();
    deliver(server, client, 3);
    client.engine.advance();
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(local).x, 6.0F) << "first target: snapped";

    // Two server ticks later: the interval is 2 * DT.
    server.registry().getComponents<ecs::Transform>().get(enemy).x = 10.0F;
    server.step();
    server.service->sent.clear();
    server.step();
    deliver(server, client, 3);
    client.engine.advance();
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(local).x, 6.0F)
        << "starts from the shown state";
    client.step();
    EXPECT_NEAR(client.registry().getComponents<ecs::Transform>().get(local).x, 8.0F, 1e-4F) << "halfway";
    client.step();
    client.step();
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(local).x, 10.0F) << "stops at the target";
}

TEST(ClientReplication, SendsAndPredictsLocalInputs) {
    Client client;
    const ecs::Entity ship = client.registry().spawnEntity();
    client.registry().addComponent(ship, ecs::Predicted{});
    std::vector<ecs::net::InputApplied> applied;
    client.engine.eventBus().subscribe<ecs::net::InputApplied>([&](const auto& ev) { applied.push_back(ev); });

    client.engine.eventBus().publish(ecs::net::LocalInput{.input = {.sequence = 1, .held = {4}, .axes = {}}});

    ASSERT_EQ(client.service->sent.size(), 1U);
    EXPECT_EQ(client.service->sent[0].peer, ecs::net::SERVER_PEER);
    EXPECT_EQ(client.service->sent[0].channel, Channel::Unreliable);
    EXPECT_EQ(client.codec->decodeInput(client.service->sent[0].data)->held, std::vector<std::uint32_t>{4});
    ASSERT_EQ(applied.size(), 1U);
    EXPECT_EQ(applied[0].entity, ship);
    EXPECT_FALSE(applied[0].replayed);
    EXPECT_EQ(client.registry().getComponents<ecs::Predicted>().get(ship).history.size(), 1U);
}

TEST(ClientReplication, SendsTheInputsNotAcknowledgedAgain) {
    Client client;
    const auto publish = [&client](const std::uint32_t sequence) {
        client.engine.eventBus().publish(ecs::net::LocalInput{.input = {.sequence = sequence, .held = {}, .axes = {}}});
    };
    const auto sentSequences = [&client] {
        std::vector<std::uint32_t> sequences;
        for (const auto& sent : client.service->sent) {
            sequences.push_back(client.codec->decodeInput(sent.data)->sequence);
        }
        client.service->sent.clear();
        return sequences;
    };

    publish(1);
    EXPECT_EQ(sentSequences(), std::vector<std::uint32_t>{1});
    publish(2);
    EXPECT_EQ(sentSequences(), (std::vector<std::uint32_t>{1, 2})) << "oldest first";
    publish(3);
    EXPECT_EQ(sentSequences(), (std::vector<std::uint32_t>{1, 2, 3}));
    publish(4);
    EXPECT_EQ(sentSequences(), (std::vector<std::uint32_t>{2, 3, 4})) << "at most INPUT_RESEND_COUNT older inputs";

    client.registry().getResource<ecs::net::SnapshotState>().lastProcessedInput = 3;
    publish(5);
    EXPECT_EQ(sentSequences(), (std::vector<std::uint32_t>{4, 5})) << "the server already applied the others";
}

TEST(ClientReplication, ALostInputIsRecoveredByTheNextDatagrams) {
    Server server;
    Client client;
    spawnPlayer(server.registry(), 3);
    std::vector<std::uint32_t> applied;
    server.engine.eventBus().subscribe<ecs::net::InputApplied>(
        [&](const ecs::net::InputApplied& ev) { applied.push_back(ev.input.sequence); });

    client.engine.eventBus().publish(ecs::net::LocalInput{.input = {.sequence = 1, .held = {}, .axes = {}}});
    client.service->sent.clear();  // the datagram of input 1 is lost
    client.engine.eventBus().publish(ecs::net::LocalInput{.input = {.sequence = 2, .held = {}, .axes = {}}});
    for (const auto& sent : client.service->sent) {
        server.source->data(3, sent.data);
    }
    server.step();
    server.step();

    EXPECT_EQ(applied, (std::vector<std::uint32_t>{1, 2}));
}

TEST(ClientReplication, ReconcilesWithTheServer) {
    Server server;
    Client client;
    const ecs::Entity serverShip = spawnPlayer(server.registry(), 3);
    // Same movement on both sides: each input moves the ship by its number of held actions.
    auto move = [](ecs::Registry& registry) {
        return [&registry](const ecs::net::InputApplied& ev) {
            registry.getComponents<ecs::Transform>().get(ev.entity).x += static_cast<float>(ev.input.held.size());
        };
    };
    server.engine.eventBus().subscribe<ecs::net::InputApplied>(move(server.registry()));
    client.engine.eventBus().subscribe<ecs::net::InputApplied>(move(client.registry()));
    client.engine.eventBus().subscribe<ecs::net::ReplicatedEntitySpawned>(
        [&](const auto& ev) { client.registry().addComponent(ev.entity, ecs::Predicted{}); });
    server.step();
    deliver(server, client, 3);
    client.step();
    const ecs::Entity ship =
        *clientEntity(client, server.registry().getComponents<ecs::NetworkId>().get(serverShip).value);

    // Three inputs predicted locally; the server only receives and applies the first one.
    for (std::uint32_t seq = 1; seq <= 3; ++seq) {
        client.engine.eventBus().publish(ecs::net::LocalInput{.input = {.sequence = seq, .held = {1}, .axes = {}}});
    }
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(ship).x, 3.0F);
    server.source->data(3, client.service->sent.at(0).data);
    server.step();
    EXPECT_FLOAT_EQ(server.registry().getComponents<ecs::Transform>().get(serverShip).x, 1.0F);

    std::vector<std::uint32_t> replayed;
    client.engine.eventBus().subscribe<ecs::net::InputApplied>([&](const auto& ev) {
        if (ev.replayed) {
            replayed.push_back(ev.input.sequence);
        }
    });
    deliver(server, client, 3);
    client.step();

    EXPECT_EQ(replayed, (std::vector<std::uint32_t>{2, 3}));
    EXPECT_FLOAT_EQ(client.registry().getComponents<ecs::Transform>().get(ship).x, 3.0F)
        << "server state (1) + the two unacknowledged inputs";
    EXPECT_EQ(client.registry().getComponents<ecs::Predicted>().get(ship).history.size(), 2U);
}

TEST(ClientReplication, ShutdownUnsubscribes) {
    Client client;
    client.engine.shutdown();

    client.engine.eventBus().publish(ecs::net::LocalInput{.input = {.sequence = 1, .held = {}, .axes = {}}});

    EXPECT_TRUE(client.service->sent.empty());
}

// --- Statistics ------------------------------------------------------------------------------------------------------

namespace {

/** @brief Gives the client a snapshot of no entity, received on its next frame. */
void receiveSnapshot(Client& client, const std::uint32_t tick, const std::uint32_t lastProcessedInput) {
    client.codec->beginSnapshot(tick);
    client.source->data(ecs::net::SERVER_PEER, client.codec->encodeSnapshot(lastProcessedInput).front());
}

void sendInput(Client& client, const std::uint32_t sequence) {
    client.engine.eventBus().publish(
        ecs::net::LocalInput{.input = NetInput{.sequence = sequence, .held = {}, .axes = {}}});
}

}  // namespace

TEST(ReplicationStats, TickJumpsCountAsMissedSnapshots) {
    Client client;
    const auto& stats = client.registry().getResource<ecs::net::ReplicationStats>();

    for (const std::uint32_t tick : {10U, 11U, 14U}) {
        receiveSnapshot(client, tick, 0);
        client.step();
    }

    EXPECT_EQ(stats.snapshotsReceived, 3U);
    EXPECT_EQ(stats.snapshotsMissed, 2U);  // 12 and 13; nothing is missed before the first snapshot
    EXPECT_EQ(stats.snapshotsStale, 0U);
}

TEST(ReplicationStats, SnapshotOlderThanTheLastOneIsStale) {
    Client client;
    const auto& stats = client.registry().getResource<ecs::net::ReplicationStats>();

    receiveSnapshot(client, 10, 0);
    client.step();
    receiveSnapshot(client, 9, 0);
    client.step();

    EXPECT_EQ(stats.snapshotsReceived, 1U);
    EXPECT_EQ(stats.snapshotsStale, 1U);
}

TEST(ReplicationStats, SnapshotInSeveralPartsCountsOnce) {
    Server server;
    Client client;
    server.codec->entitiesPerPart = 1;
    spawnPlayer(server.registry(), 3);
    spawnPlayer(server.registry(), 4);
    server.service->sent.clear();
    server.engine.eventBus().publish(ecs::net::PeerConnected{.peer = 3});
    server.step();
    deliver(server, client, 3);
    ASSERT_GE(client.source->next.size(), 2U);

    client.step();

    EXPECT_EQ(client.registry().getResource<ecs::net::ReplicationStats>().snapshotsReceived, 1U);
}

TEST(ReplicationStats, NoRoundTripBeforeAnInputIsAcknowledged) {
    Client client;
    sendInput(client, 1);
    receiveSnapshot(client, 1, 0);
    client.step();

    EXPECT_FALSE(client.registry().getResource<ecs::net::ReplicationStats>().inputRttMs.has_value());
}

TEST(ReplicationStats, RoundTripIsTheTimeUntilAnInputIsAcknowledged) {
    Client client;
    const auto& stats = client.registry().getResource<ecs::net::ReplicationStats>();

    sendInput(client, 1);
    client.now += 0.1;
    receiveSnapshot(client, 1, 1);
    client.engine.advance();
    ASSERT_TRUE(stats.inputRttMs.has_value());
    EXPECT_NEAR(*stats.inputRttMs, 100.0F, 0.01F);

    // Smoothed: one late input moves the value by a tenth of the difference.
    sendInput(client, 2);
    client.now += 0.2;
    receiveSnapshot(client, 2, 2);
    client.engine.advance();
    EXPECT_NEAR(*stats.inputRttMs, 110.0F, 0.01F);
}

TEST(ReplicationStats, SameAcknowledgementIsMeasuredOnce) {
    Client client;
    const auto& stats = client.registry().getResource<ecs::net::ReplicationStats>();

    sendInput(client, 1);
    client.now += 0.1;
    receiveSnapshot(client, 1, 1);
    client.engine.advance();

    client.now += 1.0;
    receiveSnapshot(client, 2, 1);
    client.engine.advance();

    EXPECT_NEAR(*stats.inputRttMs, 100.0F, 0.01F);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
