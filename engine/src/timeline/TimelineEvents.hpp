/**
 * @file TimelineEvents.hpp
 * @brief EventBus events published by TimelinePlugin.
 */

#pragma once

#include "timeline/TimelineEvent.hpp"

namespace timeline {

/**
 * @brief Published once for every timeline event the cursor reaches, in firing order.
 *
 * Holds a copy, so handlers may change the timeline (add events, rewind) while handling it.
 */
struct TimelineEventFired {
    /** @brief The event that fired. */
    TimelineEvent event;
};

/**
 * @brief Published once per play-through, on the step the last pending event fires.
 *
 * Only on the change from "events pending" to "finished": events added after that fire without a new
 * `TimelineFinished`, until the timeline is rewound or replaced.
 */
struct TimelineFinished {};

}  // namespace timeline
