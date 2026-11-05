#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Room.hpp"
#include "components/InputBuffer.hpp"
#include "components/NetworkId.hpp"
#include "components/NetworkOwner.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/time/Time.hpp"
#include "game/ReplicationCodec.hpp"
#include "game/ShipMovement.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkEntityMap.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"

namespace {

using ecs::net::Bytes;
using ecs::net::Channel;
using rtype::server::ClientId;
using rtype::server::MAX_PENDING_INPUTS;
using rtype::server::Room;
namespace protocol = rtype::protocol;

constexpr float FIXED_DT = 0.1F;
constexpr ClientId PLAYER = 1;
constexpr ClientId OTHER_PLAYER = 2;

/** @brief Records what the rooms send. */
class FakeServer final : public ecs::net::INetworkServer {
  public:
    struct Sent {
        ClientId client;
        Bytes data;
        Channel channel;
    };

    bool start(std::uint16_t /*port*/) override { return true; }
    void stop() override {}
    [[nodiscard]] bool isRunning() const override { return true; }
    std::vector<ecs::net::NetworkEvent> poll() override { return {}; }
    void send(const ClientId client, const Bytes& data, const Channel channel) override {
        sent.push_back({.client = client, .data = data, .channel = channel});
    }
    void broadcast(const Bytes& /*data*/, Channel /*channel*/) override { ++broadcasts; }
    void kick(ClientId /*client*/) override {}

    /** @brief Snapshots sent to a client since the last `clear()`, oldest first. */
    [[nodiscard]] std::vector<protocol::WorldState> snapshotsOf(const ClientId client) const {
        std::vector<protocol::WorldState> snapshots;
        for (const Sent& message : sent) {
            const auto decoded = protocol::decode(message.data);
            if (message.client == client && decoded && std::holds_alternative<protocol::WorldState>(*decoded)) {
                snapshots.push_back(std::get<protocol::WorldState>(*decoded));
            }
        }
        return snapshots;
    }

    std::vector<Sent> sent;
    int broadcasts = 0;
};

/** @brief Room with a fake clock: one `step()` call makes exactly one simulation step due. */
class RoomInputTest : public ::testing::Test {
  protected:
    RoomInputTest() : room(network, 2, FIXED_DT, [this] { return now; }) {
        room.addPlayer(PLAYER);
        room.start();
    }

    /** @brief Lets one fixed step of time elapse, and runs the room. */
    std::size_t step() {
        now += static_cast<double>(FIXED_DT);
        return room.advance();
    }

    void send(const std::uint32_t sequence) { send(sequence, {}); }

    void send(const std::uint32_t sequence, std::vector<protocol::AxisValue> axes) {
        room.onPacket(
            PLAYER, protocol::encode(protocol::PlayerInput{.sequence = sequence, .held = {}, .axes = std::move(axes)}));
    }

    /** @brief Position of the player's ship. */
    [[nodiscard]] ecs::Transform shipOf(const ClientId player) {
        std::optional<ecs::Transform> found;
        room.registry().runSystem<ecs::NetworkOwner, ecs::Transform>(
            [&](ecs::Entity /*entity*/, const ecs::NetworkOwner& owner, const ecs::Transform& transform) {
                if (owner.client == player) {
                    found = transform;
                }
            });
        return found.value();
    }

    double now = 0.0;
    FakeServer network;
    Room room;
};

}  // namespace

TEST_F(RoomInputTest, TwoInputsInTheSameStepAreAppliedOnTwoSteps) {
    send(1);
    send(2);

    EXPECT_EQ(step(), 1U);
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 1U);
    EXPECT_EQ(step(), 1U);
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 2U);
}

TEST_F(RoomInputTest, OutOfOrderInputsAreAppliedInOrder) {
    send(3);
    send(1);
    send(2);

    step();
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 1U);
    step();
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 2U);
    step();
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 3U);
}

TEST_F(RoomInputTest, DuplicateInputIsAppliedOnce) {
    send(1);
    send(1);

    step();
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 1U);
    step();  // nothing left in the queue: unchanged
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 1U);
}

TEST_F(RoomInputTest, AlreadyAppliedInputIsIgnored) {
    send(5);
    step();
    send(3);
    send(5);

    step();
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 5U);
}

TEST_F(RoomInputTest, FullQueueKeepsTheNewestInputs) {
    const auto total = static_cast<std::uint32_t>(MAX_PENDING_INPUTS) + 2;
    for (std::uint32_t seq = 1; seq <= total; ++seq) {
        send(seq);
    }

    // Sequences 1 and 2 were dropped: the first applied one is 3.
    step();
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 3U);
    for (std::size_t i = 1; i < MAX_PENDING_INPUTS; ++i) {
        step();
    }
    EXPECT_EQ(room.lastProcessedInput(PLAYER), total);
    step();  // queue is empty
    EXPECT_EQ(room.lastProcessedInput(PLAYER), total);
}

TEST_F(RoomInputTest, InputOfUnknownPlayerIsIgnored) {
    room.onPacket(42, protocol::encode(protocol::PlayerInput{.sequence = 1, .held = {}, .axes = {}}));
    step();
    EXPECT_EQ(room.lastProcessedInput(42), 0U);
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 0U);
}

TEST_F(RoomInputTest, PayloadsThatAreNotInputsAreIgnored) {
    room.onPacket(PLAYER, Bytes{0xFF, 0x00, 0x13});
    room.onPacket(PLAYER, Bytes{});
    room.onPacket(PLAYER, protocol::encode(protocol::Leave{}));
    room.onPacket(PLAYER, protocol::encode(protocol::WorldState{.tick = 9, .lastProcessedInput = 9, .entities = {}}));
    send(1);

    EXPECT_EQ(step(), 1U);
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 1U);
}

TEST_F(RoomInputTest, TimeResourceIsStoredInTheRegistry) {
    ASSERT_TRUE(room.registry().hasResource<ecs::time::Time>());
    EXPECT_EQ(room.registry().getResource<ecs::time::Time>().tickCount, 0U);
    step();
    EXPECT_EQ(room.registry().getResource<ecs::time::Time>().tickCount, 1U);
}

TEST_F(RoomInputTest, PlayerEntityCarriesItsNetworkComponents) {
    int found = 0;
    room.registry().runSystem<ecs::NetworkOwner, ecs::NetworkId, ecs::InputBuffer>(
        [&](ecs::Entity entity, const ecs::NetworkOwner& owner, const ecs::NetworkId& id, const ecs::InputBuffer&) {
            EXPECT_EQ(owner.client, PLAYER);
            EXPECT_EQ(room.registry().getResource<ecs::net::NetworkEntityMap>().entityOf(id.value), entity);
            ++found;
        });
    EXPECT_EQ(found, 1);
}

TEST_F(RoomInputTest, RemovedPlayerIsUnmapped) {
    room.removePlayer(PLAYER);

    EXPECT_EQ(room.registry().getResource<ecs::net::NetworkEntityMap>().size(), 0U);
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 0U);
}

TEST_F(RoomInputTest, InputMovesTheShipOfItsSender) {
    const ecs::Transform before = shipOf(PLAYER);

    send(1, {{.action = rtype::game::ACTION_MOVE_X, .value = 1.0F}});
    step();

    EXPECT_FLOAT_EQ(shipOf(PLAYER).x, before.x + (rtype::game::SHIP_SPEED * FIXED_DT));
    EXPECT_FLOAT_EQ(shipOf(PLAYER).y, before.y);
}

TEST_F(RoomInputTest, EachStepSendsASnapshotWithTheShipAndTheAcknowledgedInput) {
    send(7, {{.action = rtype::game::ACTION_MOVE_Y, .value = -1.0F}});
    step();

    const auto snapshots = network.snapshotsOf(PLAYER);
    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots[0].tick, 1U);
    EXPECT_EQ(snapshots[0].lastProcessedInput, 7U);
    ASSERT_EQ(snapshots[0].entities.size(), 1U);
    const auto& ship = snapshots[0].entities[0];
    EXPECT_NE(ship.entityId, 0U);
    EXPECT_EQ(ship.kind, rtype::game::KIND_PLAYER_SHIP);
    EXPECT_EQ(ship.subtype, 0);
    EXPECT_FLOAT_EQ(ship.x, shipOf(PLAYER).x);
    EXPECT_FLOAT_EQ(ship.y, shipOf(PLAYER).y);
    EXPECT_EQ(network.sent[0].channel, Channel::Unreliable);
}

TEST_F(RoomInputTest, CatchUpStepsSendASingleSnapshot) {
    now += static_cast<double>(FIXED_DT) * 3.5;

    EXPECT_EQ(room.advance(), 3U);

    const auto snapshots = network.snapshotsOf(PLAYER);
    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots[0].tick, 3U);
}

TEST_F(RoomInputTest, RemovedPlayerNoLongerReceivesSnapshots) {
    room.removePlayer(PLAYER);
    step();

    EXPECT_TRUE(network.snapshotsOf(PLAYER).empty());
}

TEST(Room, DoesNothingUntilStartedAndDropsThePayloads) {
    FakeServer network;
    double now = 0.0;
    Room room(network, 2, FIXED_DT, [&now] { return now; });
    room.addPlayer(PLAYER);
    room.onPacket(PLAYER, protocol::encode(protocol::PlayerInput{.sequence = 1, .held = {}, .axes = {}}));
    now += 10.0;

    EXPECT_EQ(room.advance(), 0U);
    EXPECT_TRUE(network.sent.empty());

    room.start();
    now += static_cast<double>(FIXED_DT);
    EXPECT_EQ(room.advance(), 1U);
    EXPECT_EQ(room.lastProcessedInput(PLAYER), 0U) << "the input sent before the start was dropped";
}

TEST(Room, SnapshotsStayInsideTheRoom) {
    FakeServer network;
    double now = 0.0;
    const auto clock = [&now] { return now; };
    Room first(network, 2, FIXED_DT, clock);
    Room second(network, 2, FIXED_DT, clock);
    first.addPlayer(PLAYER);
    second.addPlayer(OTHER_PLAYER);
    first.start();
    second.start();
    now += static_cast<double>(FIXED_DT);

    first.advance();

    EXPECT_EQ(network.snapshotsOf(PLAYER).size(), 1U);
    EXPECT_TRUE(network.snapshotsOf(OTHER_PLAYER).empty());
    EXPECT_EQ(network.broadcasts, 0);
    EXPECT_EQ(network.snapshotsOf(PLAYER)[0].entities.size(), 1U) << "only the entities of this room";
}

TEST(Room, PlayersKeepTheirIndexWhenAnotherOneLeaves) {
    FakeServer network;
    double now = 0.0;
    Room room(network, 3, FIXED_DT, [&now] { return now; });
    room.addPlayer(1);
    room.addPlayer(2);
    room.addPlayer(3);
    room.removePlayer(1);
    room.addPlayer(4);  // takes the free index of player 1
    room.start();
    now += static_cast<double>(FIXED_DT);
    room.advance();

    std::vector<int> indices;
    // Kept in a variable: a range-for over a member of a temporary only keeps it alive from GCC 15 on (P2718).
    const auto snapshots = network.snapshotsOf(2);
    for (const auto& entity : snapshots.at(0).entities) {
        indices.push_back(entity.subtype);
    }
    std::ranges::sort(indices);
    EXPECT_EQ(indices, (std::vector<int>{0, 1, 2}));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
