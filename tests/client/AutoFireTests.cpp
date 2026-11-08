#include <gtest/gtest.h>

#include "gameplay/AutoFire.hpp"

TEST(AutoFireLatch, APressStartsFiringAndTheNextOneStops) {
    client::AutoFireLatch latch;
    EXPECT_FALSE(latch.update(false));
    EXPECT_TRUE(latch.update(true));
    EXPECT_TRUE(latch.update(true));  // Still held: no new press.
    EXPECT_TRUE(latch.update(false));
    EXPECT_FALSE(latch.update(true));
    EXPECT_FALSE(latch.update(false));
}

TEST(AutoFireLatch, ResetStopsFiring) {
    client::AutoFireLatch latch;
    (void)latch.update(true);
    latch.reset();
    EXPECT_FALSE(latch.firing());
    EXPECT_TRUE(latch.update(true));  // The held button counts as a new press after a reset.
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
