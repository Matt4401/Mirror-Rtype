#include "net/InputQueue.hpp"

#include <algorithm>
#include <iterator>
#include <optional>

#include "components/InputBuffer.hpp"

namespace ecs::net {

bool pushInput(InputBuffer& buffer, const NetInput& input) {
    if (input.sequence <= buffer.lastProcessed || buffer.capacity == 0) {
        return false;
    }
    auto& queue = buffer.pending;
    const auto pos = std::ranges::upper_bound(queue, input.sequence, {}, &NetInput::sequence);
    if (pos != queue.begin() && std::prev(pos)->sequence == input.sequence) {
        return false;
    }
    queue.insert(pos, input);
    while (queue.size() > buffer.capacity) {
        queue.pop_front();
    }
    return true;
}

std::optional<NetInput> popInput(InputBuffer& buffer) {
    if (buffer.pending.empty()) {
        return std::nullopt;
    }
    const NetInput input = buffer.pending.front();
    buffer.pending.pop_front();
    buffer.lastProcessed = input.sequence;
    return input;
}

}  // namespace ecs::net
