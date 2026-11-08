#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "core/Engine.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "timeline/Timeline.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelineEvents.hpp"
#include "timeline/TimelinePlugin.hpp"

namespace {

timeline::TimelineEvent makeEvent(float position, const std::string& type) {
    return timeline::TimelineEvent{.position = position, .type = type, .params = {}};
}

std::vector<std::string> typesOf(std::span<const timeline::TimelineEvent> events) {
    std::vector<std::string> types;
    for (const timeline::TimelineEvent& event : events) {
        types.push_back(event.type);
    }
    return types;
}

}  // namespace

TEST(Timeline, EmptyTimelineIsFinished) {
    timeline::Timeline line;

    EXPECT_TRUE(line.finished());
    EXPECT_EQ(line.pending(), 0U);
    EXPECT_TRUE(line.advance(100.0F).empty());
    EXPECT_FLOAT_EQ(line.cursor(), 100.0F);
}

TEST(Timeline, EventFiresWhenTheCursorReachesItsPosition) {
    timeline::Timeline line;
    line.add(makeEvent(100.0F, "spawn"));

    EXPECT_TRUE(line.advanceTo(99.0F).empty());
    const auto fired = line.advanceTo(100.0F);

    ASSERT_EQ(fired.size(), 1U);
    EXPECT_EQ(fired[0].type, "spawn");
    EXPECT_TRUE(line.finished());
}

TEST(Timeline, EventFiresOnlyOnce) {
    timeline::Timeline line;
    line.add(makeEvent(10.0F, "spawn"));

    EXPECT_EQ(line.advanceTo(20.0F).size(), 1U);
    EXPECT_TRUE(line.advanceTo(30.0F).empty());
    EXPECT_TRUE(line.advance(0.0F).empty());
}

TEST(Timeline, EventsFireInPositionOrderWhateverTheInsertionOrder) {
    timeline::Timeline line;
    line.add(makeEvent(300.0F, "c"));
    line.add(makeEvent(100.0F, "a"));
    line.add(makeEvent(200.0F, "b"));

    const std::vector<std::string> expected{"a", "b", "c"};
    EXPECT_EQ(typesOf(line.advanceTo(1000.0F)), expected);
}

TEST(Timeline, EventsAtTheSamePositionKeepTheirInsertionOrder) {
    timeline::Timeline line;
    line.add(makeEvent(50.0F, "first"));
    line.add(makeEvent(50.0F, "second"));
    line.add(makeEvent(50.0F, "third"));

    const std::vector<std::string> expected{"first", "second", "third"};
    EXPECT_EQ(typesOf(line.advanceTo(50.0F)), expected);
}

TEST(Timeline, LargeStepFiresEveryEventReachedWithoutSkipping) {
    timeline::Timeline line;
    for (int i = 0; i < 100; ++i) {
        line.add(makeEvent(static_cast<float>(i) * 10.0F, "e"));
    }

    EXPECT_EQ(line.advanceTo(495.0F).size(), 50U);
    EXPECT_EQ(line.advanceTo(10000.0F).size(), 50U);
    EXPECT_TRUE(line.finished());
}

TEST(Timeline, SmallStepsFireEachEventOnceAcrossManyFrames) {
    timeline::Timeline line;
    line.add(makeEvent(1.0F, "a"));
    line.add(makeEvent(2.0F, "b"));

    std::size_t fired = 0;
    for (int frame = 0; frame < 300; ++frame) {
        fired += line.advance(1.0F / 60.0F).size();
    }

    EXPECT_EQ(fired, 2U);
}

TEST(Timeline, AdvanceAccumulatesTheCursor) {
    timeline::Timeline line;
    line.add(makeEvent(30.0F, "spawn"));

    EXPECT_TRUE(line.advance(10.0F).empty());
    EXPECT_TRUE(line.advance(10.0F).empty());
    EXPECT_EQ(line.advance(10.0F).size(), 1U);
    EXPECT_FLOAT_EQ(line.cursor(), 30.0F);
}

TEST(Timeline, CursorNeverMovesBackward) {
    timeline::Timeline line;
    line.add(makeEvent(10.0F, "spawn"));
    EXPECT_EQ(line.advanceTo(50.0F).size(), 1U);

    EXPECT_TRUE(line.advanceTo(5.0F).empty());
    EXPECT_FLOAT_EQ(line.cursor(), 50.0F);
    EXPECT_TRUE(line.advance(-20.0F).empty());
    EXPECT_FLOAT_EQ(line.cursor(), 50.0F);
}

TEST(Timeline, NanPositionLeavesTheCursorInPlace) {
    timeline::Timeline line;
    line.add(makeEvent(10.0F, "spawn"));

    EXPECT_TRUE(line.advanceTo(std::numeric_limits<float>::quiet_NaN()).empty());
    EXPECT_FLOAT_EQ(line.cursor(), 0.0F);
    EXPECT_TRUE(line.advance(std::numeric_limits<float>::quiet_NaN()).empty());
    EXPECT_EQ(line.pending(), 1U);
}

TEST(Timeline, InfinitePositionFiresEverything) {
    timeline::Timeline line;
    line.add(makeEvent(1.0F, "a"));
    line.add(makeEvent(1.0e30F, "b"));

    EXPECT_EQ(line.advanceTo(std::numeric_limits<float>::infinity()).size(), 2U);
    EXPECT_TRUE(line.finished());
}

TEST(Timeline, EventAddedBehindTheCursorFiresOnTheNextAdvance) {
    timeline::Timeline line;
    line.add(makeEvent(100.0F, "later"));
    EXPECT_TRUE(line.advanceTo(50.0F).empty());

    line.add(makeEvent(10.0F, "late"));
    const auto fired = line.advance(0.0F);

    ASSERT_EQ(fired.size(), 1U);
    EXPECT_EQ(fired[0].type, "late");
    EXPECT_EQ(line.pending(), 1U);
}

TEST(Timeline, EventAddedAheadOfTheCursorFiresInOrder) {
    timeline::Timeline line;
    line.add(makeEvent(10.0F, "a"));
    line.add(makeEvent(100.0F, "c"));
    EXPECT_EQ(line.advanceTo(20.0F).size(), 1U);

    line.add(makeEvent(60.0F, "b"));

    const std::vector<std::string> expected{"b", "c"};
    EXPECT_EQ(typesOf(line.advanceTo(200.0F)), expected);
}

TEST(Timeline, PendingCountsTheEventsNotFiredYet) {
    timeline::Timeline line;
    line.add(makeEvent(10.0F, "a"));
    line.add(makeEvent(20.0F, "b"));
    line.add(makeEvent(30.0F, "c"));

    EXPECT_EQ(line.pending(), 3U);
    (void)line.advanceTo(20.0F);
    EXPECT_EQ(line.pending(), 1U);
    EXPECT_FALSE(line.finished());
}

TEST(Timeline, RewindReplaysEveryEventInPositionOrder) {
    timeline::Timeline line;
    line.add(makeEvent(100.0F, "b"));
    (void)line.advanceTo(150.0F);
    line.add(makeEvent(10.0F, "a"));
    (void)line.advance(0.0F);

    line.rewind();

    EXPECT_FLOAT_EQ(line.cursor(), 0.0F);
    EXPECT_EQ(line.pending(), 2U);
    const std::vector<std::string> expected{"a", "b"};
    EXPECT_EQ(typesOf(line.advanceTo(1000.0F)), expected);
}

TEST(Timeline, ClearRemovesEveryEvent) {
    timeline::Timeline line;
    line.add(makeEvent(10.0F, "a"));
    (void)line.advanceTo(5.0F);

    line.clear();

    EXPECT_TRUE(line.events().empty());
    EXPECT_FLOAT_EQ(line.cursor(), 0.0F);
    EXPECT_TRUE(line.advanceTo(100.0F).empty());
}

TEST(Timeline, EventParametersAreKept) {
    timeline::Timeline line;
    line.add(timeline::TimelineEvent{
        .position = 5.0F,
        .type = "spawn",
        .params = {{"prefab", std::string("alien_scout")}, {"y", 200.0}},
    });

    const auto fired = line.advanceTo(5.0F);

    ASSERT_EQ(fired.size(), 1U);
    EXPECT_EQ(fired[0].params.find<std::string>("prefab"), "alien_scout");
    EXPECT_DOUBLE_EQ(fired[0].params.find<double>("y").value_or(0.0), 200.0);
}

// --- TimelinePlugin ---

namespace {

/** @brief Records what the plugin publishes. */
struct Recorder {
    std::vector<std::string> fired;
    int finished = 0;

    explicit Recorder(engine::Engine& engine) {
        engine.eventBus().subscribe<timeline::TimelineEventFired>(
            [this](const timeline::TimelineEventFired& fired) { this->fired.push_back(fired.event.type); });
        engine.eventBus().subscribe<timeline::TimelineFinished>(
            [this](const timeline::TimelineFinished& /*finished*/) { ++this->finished; });
    }
};

ecs::time::Time fixedStep(float dt) { return ecs::time::Time{.deltaTime = dt, .totalElapsedTime = 0.0F}; }

}  // namespace

TEST(TimelinePlugin, IsStoppedByDefault) {
    engine::Engine engine;
    timeline::TimelinePlugin player;
    player.timeline().add(makeEvent(0.5F, "spawn"));
    const Recorder recorder(engine);

    player.fixedUpdate(engine, fixedStep(1.0F));

    EXPECT_FLOAT_EQ(player.speed(), 0.0F);
    EXPECT_TRUE(recorder.fired.empty());
}

TEST(TimelinePlugin, MovesTheCursorBySpeedTimesDeltaTime) {
    engine::Engine engine;
    timeline::TimelinePlugin player;
    player.timeline().add(makeEvent(120.0F, "spawn"));
    player.setSpeed(60.0F);
    const Recorder recorder(engine);

    player.fixedUpdate(engine, fixedStep(1.0F));
    EXPECT_TRUE(recorder.fired.empty());
    EXPECT_FLOAT_EQ(player.timeline().cursor(), 60.0F);

    player.fixedUpdate(engine, fixedStep(1.0F));
    ASSERT_EQ(recorder.fired.size(), 1U);
    EXPECT_EQ(recorder.fired[0], "spawn");
}

TEST(Timeline, AddAllGivesTheSameOrderAsAddingOneByOne) {
    const std::vector<timeline::TimelineEvent> events{makeEvent(300.0F, "c"), makeEvent(100.0F, "a"),
                                                      makeEvent(300.0F, "d"), makeEvent(200.0F, "b")};
    timeline::Timeline oneByOne;
    for (const timeline::TimelineEvent& event : events) {
        oneByOne.add(event);
    }
    timeline::Timeline atOnce;
    atOnce.addAll(events);

    EXPECT_EQ(typesOf(atOnce.events()), typesOf(oneByOne.events()));
    EXPECT_EQ(typesOf(atOnce.events()), (std::vector<std::string>{"a", "b", "c", "d"}));
}

TEST(Timeline, AddAllKeepsTheFiredEventsInPlace) {
    timeline::Timeline line;
    line.add(makeEvent(100.0F, "first"));
    (void)line.advanceTo(150.0F);
    line.addAll({makeEvent(400.0F, "late"), makeEvent(120.0F, "behind")});

    EXPECT_EQ(typesOf(line.advanceTo(150.0F)), std::vector<std::string>{"behind"});
    EXPECT_EQ(typesOf(line.advanceTo(400.0F)), std::vector<std::string>{"late"});
}

TEST(Timeline, CopiesShareTheirEventsButNotTheirCursor) {
    timeline::Timeline level;
    level.addAll({makeEvent(100.0F, "a"), makeEvent(200.0F, "b")});
    timeline::Timeline first = level;
    timeline::Timeline second = level;

    EXPECT_EQ(first.events().data(), level.events().data()) << "copying does not copy the events";
    EXPECT_EQ(typesOf(first.advanceTo(250.0F)), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(typesOf(second.advanceTo(100.0F)), std::vector<std::string>{"a"});
    EXPECT_EQ(level.pending(), 2U);
}

TEST(Timeline, ChangingACopyLeavesTheOthersUnchanged) {
    timeline::Timeline level;
    level.add(makeEvent(100.0F, "a"));
    timeline::Timeline copy = level;
    copy.add(makeEvent(50.0F, "added"));

    EXPECT_EQ(typesOf(level.events()), std::vector<std::string>{"a"});
    EXPECT_EQ(typesOf(copy.events()), (std::vector<std::string>{"added", "a"}));
}

TEST(TimelinePlugin, PublishesEveryReachedEventInOrderThenFinishedOnce) {
    engine::Engine engine;
    timeline::TimelinePlugin player;
    player.timeline().add(makeEvent(10.0F, "a"));
    player.timeline().add(makeEvent(20.0F, "b"));
    const Recorder recorder(engine);

    player.step(engine, 100.0F);
    player.step(engine, 100.0F);

    const std::vector<std::string> expected{"a", "b"};
    EXPECT_EQ(recorder.fired, expected);
    EXPECT_EQ(recorder.finished, 1);
}

TEST(TimelinePlugin, ZeroSpeedPausesPlayback) {
    engine::Engine engine;
    timeline::TimelinePlugin player;
    player.timeline().add(makeEvent(50.0F, "spawn"));
    player.setSpeed(100.0F);
    const Recorder recorder(engine);

    player.setSpeed(0.0F);
    for (int i = 0; i < 100; ++i) {
        player.fixedUpdate(engine, fixedStep(1.0F / 60.0F));
    }

    EXPECT_TRUE(recorder.fired.empty());
}

TEST(TimelinePlugin, InvalidSpeedsAreIgnored) {
    timeline::TimelinePlugin player;
    player.setSpeed(30.0F);

    player.setSpeed(-5.0F);
    player.setSpeed(std::numeric_limits<float>::quiet_NaN());
    player.setSpeed(std::numeric_limits<float>::infinity());

    EXPECT_FLOAT_EQ(player.speed(), 30.0F);
}

TEST(TimelinePlugin, HandlerMayAddEventsWhileHandlingOne) {
    engine::Engine engine;
    timeline::TimelinePlugin player;
    player.timeline().add(makeEvent(10.0F, "first"));
    player.timeline().add(makeEvent(20.0F, "second"));
    std::vector<std::string> seen;
    engine.eventBus().subscribe<timeline::TimelineEventFired>([&](const timeline::TimelineEventFired& fired) {
        seen.push_back(fired.event.type);
        if (fired.event.type == "first") {
            for (int i = 0; i < 64; ++i) {  // forces the event storage to reallocate
                player.timeline().add(makeEvent(1000.0F + static_cast<float>(i), "added"));
            }
        }
    });

    player.step(engine, 30.0F);

    const std::vector<std::string> expected{"first", "second"};
    EXPECT_EQ(seen, expected);
    EXPECT_EQ(player.timeline().pending(), 64U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
