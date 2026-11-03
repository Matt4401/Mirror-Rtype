#pragma once

#include <vector>

#include "INetworkManager.hpp"

namespace ecs::net {

/**
 * @brief Where `NetworkPlugin` reads incoming network events from.
 *
 * On a client, the transport itself (see `ClientConnection`). On a server running several engines, a queue filled by
 * the code that owns the transport, so that each engine only receives the events of its own peers.
 */
class INetworkEventSource {
  public:
    /** @brief Default constructor. */
    INetworkEventSource() = default;
    /** @brief Virtual destructor. */
    virtual ~INetworkEventSource() = default;
    /** @brief Deleted copy constructor: a source hands out each event once. */
    INetworkEventSource(const INetworkEventSource&) = delete;
    /** @brief Deleted copy assignment operator. */
    INetworkEventSource& operator=(const INetworkEventSource&) = delete;
    /** @brief Deleted move constructor: sources are shared through `std::shared_ptr`. */
    INetworkEventSource(INetworkEventSource&&) = delete;
    /** @brief Deleted move assignment operator. */
    INetworkEventSource& operator=(INetworkEventSource&&) = delete;

    /**
     * @brief Takes every event received since the last call. Called once per frame by `NetworkPlugin::preUpdate`.
     * @return Events in arrival order.
     */
    virtual std::vector<NetworkEvent> poll() = 0;
};

}  // namespace ecs::net
