#pragma once

#include "INetworkManager.hpp"

namespace engine {

/**
 * @brief Abstract network service exposed through the Mediator, used by systems to send messages.
 *
 * Registered by `ecs::net::NetworkPlugin`. On a client, the only peer is `ecs::net::SERVER_PEER`.
 */
class INetworkService {
  public:
    /** @brief Default constructor. */
    INetworkService() = default;

    /** @brief Virtual destructor. */
    virtual ~INetworkService() = default;

    /** @brief Deleted copy constructor: a service is shared through `std::shared_ptr`, never copied. */
    INetworkService(const INetworkService&) = delete;

    /** @brief Deleted copy assignment operator. */
    INetworkService& operator=(const INetworkService&) = delete;

    /** @brief Deleted move constructor: the Mediator holds a pointer to the service. */
    INetworkService(INetworkService&&) = delete;

    /** @brief Deleted move assignment operator. */
    INetworkService& operator=(INetworkService&&) = delete;

    /**
     * @brief Sends a message to one peer. Thread-safe; dropped if the peer is unknown or the payload too large.
     * @param peer Destination peer.
     * @param data Payload (at most `NetworkConfig::maxReliablePayload` bytes, or `MAX_UNRELIABLE_PAYLOAD`).
     * @param channel Delivery guarantee.
     */
    virtual void send(ecs::net::PeerId peer, const ecs::net::Bytes& data, ecs::net::Channel channel) = 0;
    /**
     * @brief Sends a message to every connected peer. Thread-safe.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    virtual void broadcast(const ecs::net::Bytes& data, ecs::net::Channel channel) = 0;
    /**
     * @brief Closes a peer's connection; a `Disconnected` event follows. Thread-safe.
     * @param peer Peer to disconnect.
     */
    virtual void kick(ecs::net::PeerId peer) = 0;
};

}  // namespace engine
