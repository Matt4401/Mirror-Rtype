#include "EventQueue.hpp"

#include <mutex>
#include <utility>
#include <vector>

#include "Network/INetworkManager.hpp"

namespace rtype::net::queue {

void EventQueue::push(NetworkEvent event) {
    const std::scoped_lock lock(_mutex);
    _events.push_back(std::move(event));
}

std::vector<NetworkEvent> EventQueue::popAll() {
    std::vector<NetworkEvent> events;
    {
        const std::scoped_lock lock(_mutex);
        events.swap(_events);
    }
    return events;
}

}  // namespace rtype::net::queue
