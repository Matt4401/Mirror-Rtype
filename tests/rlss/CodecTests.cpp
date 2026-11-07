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
    EXPECT_FALSE(codec.decodeInput(rlss::protocol::encode(rlss::protocol::CreateMatch{})).has_value());
}

TEST(RlssCodec, SnapshotSplitsIntoPartsAndRecreatesTheCars) {
    ecs::Registry server;
    constexpr std::uint16_t MATCH = 12;
    game::ReplicationCodec codec(MATCH);
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
        // Every part says which match it comes from: a client drops those of another match.
        EXPECT_EQ(rlss::protocol::snapshotMatch(part), MATCH);
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

TEST(RlssCodec, SoundCuesReachTheClient) {
    ecs::Registry server;
    game::ReplicationCodec codec(1);
    codec.beginSnapshot(1);
    const ecs::Entity car = server.spawnEntity();
    server.addComponent(car, ecs::Transform{});
    server.addComponent(car, game::CarCues{.jumps = 18, .jumpKind = 2, .impacts = 3, .impactSpeed = 1200.0F});
    codec.writeEntity(server, car, 1, ecs::Replicated{.kind = game::KIND_CAR});
    const ecs::Entity ball = server.spawnEntity();
    server.addComponent(ball, ecs::Transform{});
    server.addComponent(ball, game::BallCues{.hits = 5, .hitSpeed = 9000.0F, .bounces = 1, .bounceSpeed = 50.0F});
    codec.writeEntity(server, ball, 2, ecs::Replicated{.kind = game::KIND_BALL});
    const auto parts = codec.encodeSnapshot(0);
    ASSERT_EQ(parts.size(), 1U);

    ecs::Registry client;
    ecs::net::NetworkEntityMap map;
    ASSERT_TRUE(codec.decodeSnapshot(parts.front()).has_value());
    ecs::net::SnapshotApplier applier(client, map, 1.0F / 60.0F);
    codec.applySnapshot(applier);

    const auto carEntity = map.entityOf(1);
    const auto ballEntity = map.entityOf(2);
    ASSERT_TRUE(carEntity.has_value());
    ASSERT_TRUE(ballEntity.has_value());
    const game::CarCues& carCues = client.getComponents<game::CarCues>().get(*carEntity);
    EXPECT_EQ(carCues.jumps, 18 % 16);  // the low 4 bits travel
    EXPECT_EQ(carCues.jumpKind, 2);
    EXPECT_EQ(carCues.impacts, 3);
    EXPECT_NEAR(carCues.impactSpeed, 1200.0F, game::SOUND_CUE_FULL_SPEED / 30.0F);  // rounded to 16 levels
    const game::BallCues& ballCues = client.getComponents<game::BallCues>().get(*ballEntity);
    EXPECT_EQ(ballCues.hits, 5);
    EXPECT_FLOAT_EQ(ballCues.hitSpeed, game::SOUND_CUE_FULL_SPEED);  // capped at the strongest level
    EXPECT_EQ(ballCues.bounces, 1);
    EXPECT_NEAR(ballCues.bounceSpeed, 0.0F, game::SOUND_CUE_FULL_SPEED / 30.0F);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
