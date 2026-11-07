/**
 * @file Timeline.hpp
 * @brief Sequence of events fired in order as a cursor moves forward.
 */

#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "timeline/TimelineEvent.hpp"

namespace timeline {

/**
 * @brief Events sorted by position, fired once each as a cursor moves forward.
 *
 * The cursor only moves forward: `advanceTo` returns every event whose position was reached since the previous call,
 * in position order (events at the same position keep their insertion order). However large the step, no event is
 * skipped and none fires twice. A pure data structure: no clock, no engine, no game knowledge.
 *
 * @code
 * timeline::Timeline level;
 * level.add({.position = 400.0F, .type = "spawn", .params = {{"prefab", "alien_scout"}}});
 * for (const timeline::TimelineEvent& event : level.advance(scrollSpeed * dt)) {
 *     handle(event);
 * }
 * @endcode
 */
class Timeline {
  public:
    /** @brief Creates an empty timeline with its cursor at 0. */
    Timeline() = default;

    /**
     * @brief Adds an event, keeping the events sorted by position.
     *
     * An event added behind the cursor would never fire, so it fires on the next `advanceTo`.
     *
     * @param event Event to add.
     */
    void add(TimelineEvent event);

    /**
     * @brief Moves the cursor forward to a position and returns the events reached.
     *
     * A position behind the cursor leaves it where it is.
     *
     * @param position New cursor position.
     * @return Events reached by this move, in order; valid until the next call that changes the timeline.
     */
    [[nodiscard]] std::span<const TimelineEvent> advanceTo(float position);

    /**
     * @brief Moves the cursor forward by a distance and returns the events reached.
     * @param delta Distance to move by; a negative or zero delta fires only events added behind the cursor.
     * @return Events reached by this move, in order.
     */
    [[nodiscard]] std::span<const TimelineEvent> advance(float delta);

    /**
     * @brief Puts the cursor back to 0, so every event can fire again, in position order.
     */
    void rewind() noexcept;

    /**
     * @brief Removes every event and puts the cursor back to 0.
     */
    void clear() noexcept;

    /**
     * @brief Gets the cursor position.
     * @return Current position.
     */
    [[nodiscard]] float cursor() const noexcept { return _cursor; }

    /**
     * @brief Gets every event, fired or not, in firing order.
     *
     * This is the position order, except that an event added behind the cursor comes after the events already fired.
     *
     * @return The events.
     */
    [[nodiscard]] std::span<const TimelineEvent> events() const noexcept { return _events; }

    /**
     * @brief Counts the events that have not fired yet.
     * @return Number of pending events.
     */
    [[nodiscard]] std::size_t pending() const noexcept { return _events.size() - _next; }

    /**
     * @brief Tells whether every event has fired.
     * @return True if no event is pending (also true for an empty timeline).
     */
    [[nodiscard]] bool finished() const noexcept { return _next == _events.size(); }

  private:
    /** @brief Events in firing order. */
    std::vector<TimelineEvent> _events;
    /** @brief Index of the first event that has not fired. */
    std::size_t _next = 0;
    /** @brief Cursor position. */
    float _cursor = 0.0F;
};

}  // namespace timeline
