#include "timeline/Timeline.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <span>
#include <utility>

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

void Timeline::add(TimelineEvent event) {
    // Only the pending part is searched: every pending event is ahead of the cursor, so an event added behind it
    // lands first among them and fires on the next advance. Ties keep their insertion order.
    const auto pendingBegin = std::next(_events.begin(), static_cast<std::ptrdiff_t>(_next));
    const auto where = std::upper_bound(pendingBegin, _events.end(), event, byPosition);
    _events.insert(where, std::move(event));
}

std::span<const TimelineEvent> Timeline::advanceTo(float position) {
    // std::max keeps its first argument when the comparison is false, so a NaN position leaves the cursor in place.
    _cursor = std::max(_cursor, position);
    const std::size_t first = _next;
    while (_next < _events.size() && _events.at(_next).position <= _cursor) {
        ++_next;
    }
    return std::span<const TimelineEvent>(_events).subspan(first, _next - first);
}

std::span<const TimelineEvent> Timeline::advance(float delta) { return advanceTo(_cursor + delta); }

void Timeline::rewind() noexcept {
    // Events added behind the cursor were placed by firing order: restore the position order before replaying.
    std::ranges::stable_sort(_events, byPosition);
    _next = 0;
    _cursor = 0.0F;
}

void Timeline::clear() noexcept {
    _events.clear();
    _next = 0;
    _cursor = 0.0F;
}

}  // namespace timeline
