#include "net/asio/EventQueue.hpp"

#include <mutex>
#include <utility>
#include <vector>

#include "net/INetworkManager.hpp"

namespace ecs::net::detail {

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

}  // namespace ecs::net::detail
