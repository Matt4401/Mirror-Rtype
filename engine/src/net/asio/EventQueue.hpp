#pragma once

#include <mutex>
#include <vector>

#include "net/INetworkManager.hpp"

namespace ecs::net::detail {

/** @brief Hands events from the network thread to the game thread. */
class EventQueue {
  public:
    /** @brief Adds an event (network thread). */
    void push(NetworkEvent event);
    /** @brief Takes every queued event, oldest first (game thread). */
    [[nodiscard]] std::vector<NetworkEvent> popAll();

  private:
    /** @brief Guards `_events`. */
    std::mutex _mutex;
    /** @brief Events not polled yet. */
    std::vector<NetworkEvent> _events;
};

}  // namespace ecs::net::detail
