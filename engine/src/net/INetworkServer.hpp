#pragma once

#include <cstdint>

#include "net/INetworkManager.hpp"

namespace ecs::net {

/** @brief Server side of the transport: accepts peers and exchanges messages with them. */
class INetworkServer : public INetworkManager {
  public:
    /**
     * @brief Listens for peers on a TCP and a UDP port (the same number), and starts the network thread.
     * @param port Port to listen on.
     * @return False if the port is taken or the server was already started.
     */
    virtual bool start(std::uint16_t port) = 0;
    /**
     * @brief Sends a message to one peer. Thread-safe; dropped if the peer is unknown or the payload too large.
     * @param peer Destination peer.
     * @param data Payload (at most `NetworkConfig::maxReliablePayload` bytes, or `MAX_UNRELIABLE_PAYLOAD`).
     * @param channel Delivery guarantee.
     */
    virtual void send(PeerId peer, const Bytes& data, Channel channel) = 0;
    /**
     * @brief Sends a message to every connected peer. Thread-safe.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    virtual void broadcast(const Bytes& data, Channel channel) = 0;
    /**
     * @brief Closes a peer's connection; a `Disconnected` event follows. Thread-safe.
     * @param peer Peer to disconnect.
     */
    virtual void kick(PeerId peer) = 0;
};

}  // namespace ecs::net
