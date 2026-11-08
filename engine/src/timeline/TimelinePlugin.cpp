/**
 * @file TimelinePlugin.cpp
 * @brief TimelinePlugin implementation.
 */

#include "timeline/TimelinePlugin.hpp"

#include <cmath>
#include <span>
#include <vector>

#include "core/Engine.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "timeline/TimelineEvent.hpp"
#include "timeline/TimelineEvents.hpp"

namespace timeline {

void TimelinePlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    step(engine, _speed * time.deltaTime);
}

void TimelinePlugin::step(engine::Engine& engine, float delta) {
    const bool wasFinished = _timeline.finished();
    const std::span<const TimelineEvent> reached = _timeline.advance(delta);
    if (reached.empty()) {
        return;
    }
    // Copied before publishing: a handler may add events, which would invalidate the span.
    const std::vector<TimelineEvent> fired(reached.begin(), reached.end());
    ecs::EventBus& bus = engine.eventBus();
    for (const TimelineEvent& event : fired) {
        bus.publish(TimelineEventFired{.event = event});
    }
    if (!wasFinished && _timeline.finished()) {
        bus.publish(TimelineFinished{});
    }
}

void TimelinePlugin::setSpeed(float unitsPerSecond) noexcept {
    if (std::isfinite(unitsPerSecond) && unitsPerSecond >= 0.0F) {
        _speed = unitsPerSecond;
    }
}

}  // namespace timeline
