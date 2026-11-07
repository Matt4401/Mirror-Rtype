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

/** @brief Published once, on the step the last pending event fires. */
struct TimelineFinished {};

}  // namespace timeline
