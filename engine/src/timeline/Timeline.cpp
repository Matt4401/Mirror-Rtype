#include "timeline/Timeline.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "timeline/TimelineEvent.hpp"

namespace timeline {

namespace {

/**
 * @brief Orders events by position.
 * @param lhs First event.
 * @param rhs Second event.
 * @return True if `lhs` comes strictly before `rhs`.
 */
bool byPosition(const TimelineEvent& lhs, const TimelineEvent& rhs) noexcept { return lhs.position < rhs.position; }

}  // namespace

std::vector<TimelineEvent>& Timeline::ownEvents() {
    if (_events == nullptr) {
        _events = std::make_shared<std::vector<TimelineEvent>>();
    } else if (_events.use_count() > 1) {
        _events = std::make_shared<std::vector<TimelineEvent>>(*_events);
    }
    return *_events;
}

std::span<const TimelineEvent> Timeline::events() const noexcept {
    if (_events == nullptr) {
        return {};
    }
    return *_events;
}

void Timeline::add(TimelineEvent event) {
    std::vector<TimelineEvent>& events = ownEvents();
    // Only the pending part is searched: every pending event is ahead of the cursor, so an event added behind it
    // lands first among them and fires on the next advance. Ties keep their insertion order.
    const auto pendingBegin = std::next(events.begin(), static_cast<std::ptrdiff_t>(_next));
    const auto where = std::upper_bound(pendingBegin, events.end(), event, byPosition);
    events.insert(where, std::move(event));
}

void Timeline::addAll(std::vector<TimelineEvent> added) {
    if (added.empty()) {
        return;
    }
    std::vector<TimelineEvent>& events = ownEvents();
    events.reserve(events.size() + added.size());
    std::ranges::move(added, std::back_inserter(events));
    // Stable: the pending events keep their order, and come before the added ones at the same position, as with add().
    std::stable_sort(std::next(events.begin(), static_cast<std::ptrdiff_t>(_next)), events.end(), byPosition);
}

std::span<const TimelineEvent> Timeline::advanceTo(float position) {
    // std::max keeps its first argument when the comparison is false, so a NaN position leaves the cursor in place.
    _cursor = std::max(_cursor, position);
    const std::span<const TimelineEvent> all = events();
    const std::size_t first = _next;
    while (_next < all.size() && all[_next].position <= _cursor) {
        ++_next;
    }
    return all.subspan(first, _next - first);
}

std::span<const TimelineEvent> Timeline::advance(float delta) { return advanceTo(_cursor + delta); }

void Timeline::rewind() {
    // Events added behind the cursor were placed by firing order: restore the position order before replaying. Only
    // then are the events copied, so rewinding a shared, already sorted timeline costs nothing.
    if (!std::ranges::is_sorted(events(), byPosition)) {
        std::ranges::stable_sort(ownEvents(), byPosition);
    }
    _next = 0;
    _cursor = 0.0F;
}

void Timeline::clear() noexcept {
    _events.reset();
    _next = 0;
    _cursor = 0.0F;
}

}  // namespace timeline
