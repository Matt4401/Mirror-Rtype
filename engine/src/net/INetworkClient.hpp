#pragma once

#include <cstdint>
#include <string>

#include "net/INetworkManager.hpp"

namespace ecs::net {

/** @brief Client side of the transport: one connection to a server. */
class INetworkClient : public INetworkManager {
  public:
    /**
     * @brief Starts connecting in the background and returns at once: a slow server never blocks the caller.
     *
     * A `Connected` event follows once the server accepted the transport handshake (same
     * `NetworkConfig::protocolId`). A `Disconnected` event follows instead if the server cannot be reached or refuses
     * the connection, and `disconnectReason()` says why. A client that failed cannot connect again: create another one.
     *
     * @param host Server name or address.
     * @param port Server TCP port.
     * @return False if the client was already started.
     */
    virtual bool connect(const std::string& host, std::uint16_t port) = 0;
    /**
     * @brief Tells why the connection ended, to show it to the player. Thread-safe.
     * @return The reason; only meaningful after a `Disconnected` event.
     */
    [[nodiscard]] virtual DisconnectReason disconnectReason() const = 0;
    /**
     * @brief Opens the unreliable channel to the server. Only valid after `Connected`.
     * @param port Server UDP port (sent by the game's own handshake).
     * @return False if not connected yet or the socket cannot be opened.
     */
    virtual bool openUdp(std::uint16_t port) = 0;
    /** @brief Closes the unreliable channel; the connection itself stays open. */
    virtual void closeUdp() = 0;
    /**
     * @brief Sends a message to the server. Thread-safe; dropped if the payload is too large or if not connected.
     * @param data Payload.
     * @param channel Delivery guarantee; `Unreliable` is dropped while UDP is closed.
     */
    virtual void send(const Bytes& data, Channel channel) = 0;
};

}  // namespace ecs::net
