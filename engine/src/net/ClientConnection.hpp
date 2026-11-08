#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "INetworkEventSource.hpp"
#include "INetworkManager.hpp"
#include "INetworkService.hpp"
#include "net/INetworkClient.hpp"

namespace ecs::net {

/**
 * @brief Adapts a client transport to `NetworkPlugin`: it is both the event source and the network service.
 *
 * A client only talks to the server, so every message goes to `SERVER_PEER`.
 *
 * The transport can be replaced (`setClient`), for instance by a fresh one for each connection attempt, since a
 * transport connects only once. Without a transport, sends are dropped and `poll()` returns nothing.
 */
class ClientConnection final : public INetworkService, public INetworkEventSource {
  public:
    /**
     * @brief Wraps a client transport.
     * @param client Transport, shared with the game so that it can still call `connect()` and `openUdp()`; may be null.
     */
    explicit ClientConnection(std::shared_ptr<INetworkClient> client) : _client(std::move(client)) {}
    /**
     * @brief Sends a message to the server; dropped if `peer` is not `SERVER_PEER`.
     * @param peer Must be `SERVER_PEER`.
     * @param data Payload.
     * @param ch Delivery guarantee.
     */
    void send(PeerId peer, const Bytes& data, Channel ch) override;
    /**
     * @brief Sends a message to the server, the only peer of a client.
     * @param data Payload.
     * @param ch Delivery guarantee.
     */
    void broadcast(const Bytes& data, Channel ch) override;
    /**
     * @brief Takes the events received from the server.
     * @return Events in arrival order.
     */
    [[nodiscard]] std::vector<NetworkEvent> poll() override;
    /**
     * @brief Disconnects from the server.
     * @param peer Ignored.
     */
    void kick(PeerId peer) override;

    /**
     * @brief Replaces the wrapped transport.
     * @param client New transport, or null to have none.
     */
    void setClient(std::shared_ptr<INetworkClient> client) { _client = std::move(client); }

    /**
     * @brief Gets the wrapped transport.
     * @return The transport, or null.
     */
    [[nodiscard]] const std::shared_ptr<INetworkClient>& client() const noexcept { return _client; }

    /**
     * @brief Gets the traffic counters of the wrapped transport.
     * @return Its counters, or zeros without a transport.
     */
    [[nodiscard]] TransportStats stats() const { return _client ? _client->stats() : TransportStats{}; }

  private:
    /** @brief Wrapped transport. */
    std::shared_ptr<INetworkClient> _client;
};

}  // namespace ecs::net
