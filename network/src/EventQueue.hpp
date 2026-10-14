#pragma once

#include <mutex>
#include <vector>

#include "Network/INetworkManager.hpp"

namespace rtype::net::queue {

class EventQueue {
  public:
    void push(NetworkEvent event);
    [[nodiscard]] std::vector<NetworkEvent> popAll();

  private:
    std::mutex _mutex;
    std::vector<NetworkEvent> _events;
};

}  // namespace rtype::net::queue
