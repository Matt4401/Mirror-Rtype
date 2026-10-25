#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "Room.hpp"
#include "ecs/time/Clock.hpp"
#include "ecs/time/Time.hpp"

namespace {

using rtype::server::ClientId;
using rtype::server::MAX_PENDING_INPUTS;
using rtype::server::PlayerInput;
using rtype::server::Room;

constexpr float FIXED_DT = 0.1F;
constexpr ClientId PLAYER = 1;

/** @brief Room with a fake clock: one `step()` call makes exactly one simulation step due. */
class RoomInputTest : public ::testing::Test {
  protected:
    RoomInputTest() : room(2, FIXED_DT, [this] { return now; }) {
        room.addPlayer(PLAYER);
        room.start();
    }

    /** @brief Lets one fixed step of time elapse, and runs the room. */
    std::size_t step() {
        now += static_cast<double>(FIXED_DT);
        return room.advance();
    }

    void send(const std::uint32_t sequence) { room.onInput(PLAYER, PlayerInput{.sequence = sequence, .actions = 0}); }

    double now = 0.0;
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
    const auto total = static_cast<std::uint32_t>(MAX_PENDING_INPUTS) + 2;  // sequences 1..10
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
    room.onInput(42, PlayerInput{.sequence = 1, .actions = 0});
    EXPECT_EQ(room.lastProcessedInput(42), 0U);
}

TEST_F(RoomInputTest, TimeResourceIsStoredInTheRegistry) {
    EXPECT_FALSE(room.registry().hasResource<ecs::time::Time>());
    step();
    ASSERT_TRUE(room.registry().hasResource<ecs::time::Time>());
    EXPECT_EQ(room.registry().getResource<ecs::time::Time>().tickCount, 1U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
