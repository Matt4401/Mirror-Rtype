#include <gtest/gtest.h>

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "components/InputBuffer.hpp"
#include "components/Interpolated.hpp"
#include "components/Replicated.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "game/ReplicationCodec.hpp"
#include "net/NetworkEntityMap.hpp"
#include "net/SnapshotApplier.hpp"
#include "protocol/Protocol.hpp"

namespace game = rlss::game;

TEST(RlssCodec, InputSurvivesTheWire) {
    game::ReplicationCodec codec;
    const ecs::NetInput input{
        .sequence = 42, .held = {game::JUMP_ID}, .axes = {{.action = game::STEER_ID, .value = 0.5F}}};
    const auto decoded = codec.decodeInput(codec.encodeInput(input));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, 42U);
    EXPECT_EQ(decoded->held, std::vector<std::uint32_t>{game::JUMP_ID});
    ASSERT_EQ(decoded->axes.size(), 1U);
    EXPECT_NEAR(decoded->axes[0].value, 0.5F, 0.01F);
    // A snapshot is not an input.
    EXPECT_FALSE(codec.decodeInput(rlss::protocol::encode(rlss::protocol::LobbyState{})).has_value());
}

TEST(RlssCodec, SnapshotSplitsIntoPartsAndRecreatesTheCars) {
    ecs::Registry server;
    game::ReplicationCodec codec;
    codec.beginSnapshot(7);
    constexpr std::uint32_t CARS = rlss::protocol::MAX_ENTITIES_PER_PART + 2;
    for (std::uint32_t id = 1; id <= CARS; ++id) {
        const ecs::Entity car = server.spawnEntity();
        server.addComponent(car, ecs::Transform{.x = static_cast<float>(id), .y = 2.0F, .scaleX = -1.0F});
        server.addComponent(car, game::CarLook{.color = 4});
        codec.writeEntity(server, car, id, ecs::Replicated{.kind = game::KIND_CAR});
    }
    const auto parts = codec.encodeSnapshot(3);
    ASSERT_EQ(parts.size(), 2U);

    ecs::Registry client;
    ecs::net::NetworkEntityMap map;
    std::size_t applied = 0;
    for (const auto& part : parts) {
        const auto header = codec.decodeSnapshot(part);
        ASSERT_TRUE(header.has_value());
        EXPECT_EQ(header->tick, 7U);
        EXPECT_EQ(header->lastProcessedInput, 3U);
        EXPECT_EQ(header->partCount, 2U);
        ecs::net::SnapshotApplier applier(client, map, 1.0F / 60.0F);
        codec.applySnapshot(applier);
        applied += applier.spawned().size();
    }
    EXPECT_EQ(applied, CARS);

    const auto entity = map.entityOf(1);
    ASSERT_TRUE(entity.has_value());
    EXPECT_TRUE(client.hasComponent<ecs::Interpolated>(*entity));
    EXPECT_EQ(client.getComponents<game::CarLook>().get(*entity).color, 4);
    EXPECT_FLOAT_EQ(client.getComponents<ecs::Transform>().get(*entity).scaleX, -1.0F);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
