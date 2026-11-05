#pragma once

#include <cstdint>
#include <string>

#include "net/INetworkManager.hpp"

namespace ecs::net {

/** @brief Client side of the transport: one connection to a server. */
class INetworkClient : public INetworkManager {
  public:
    /**
     * @brief Connects over TCP and starts the transport handshake. Blocks until the TCP connection is made.
     *
     * A `Connected` event follows once the server accepted the handshake (same `NetworkConfig::protocolId`);
     * a `Disconnected` event follows instead if it refused it.
     *
     * @param host Server name or address.
     * @param port Server TCP port.
     * @return False if the server cannot be reached or the client was already started.
     */
    virtual bool connect(const std::string& host, std::uint16_t port) = 0;
    /**
     * @brief Opens the unreliable channel to the server. Only valid after `Connected`.
     * @param port Server UDP port (sent by the game's own handshake).
     * @return False if not connected yet or the socket cannot be opened.
     */
    virtual bool openUdp(std::uint16_t port) = 0;
    /** @brief Closes the unreliable channel; the connection itself stays open. */
    virtual void closeUdp() = 0;
    /**
     * @brief Sends a message to the server. Thread-safe; dropped if the payload is too large.
     * @param data Payload.
     * @param channel Delivery guarantee; `Unreliable` is dropped while UDP is closed.
     */
    virtual void send(const Bytes& data, Channel channel) = 0;
};

}  // namespace ecs::net
