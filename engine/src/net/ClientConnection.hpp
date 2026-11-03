#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "INetworkEventSource.hpp"
#include "INetworkManager.hpp"
#include "INetworkService.hpp"
#include "net/INetworkClient.hpp"

namespace engine {

/**
 * @brief Adapts a client transport to `NetworkPlugin`: it is both the event source and the network service.
 *
 * A client only talks to the server, so every message goes to `ecs::net::SERVER_PEER`.
 */
class ClientConnection final : public INetworkService, public ecs::net::INetworkEventSource {
  public:
    /**
     * @brief Wraps a client transport.
     * @param client Transport, shared with the game so that it can still call `connect()` and `openUdp()`.
     */
    explicit ClientConnection(std::shared_ptr<ecs::net::INetworkClient> client) : _client(std::move(client)) {}
    /**
     * @brief Sends a message to the server; dropped if `peer` is not `SERVER_PEER`.
     * @param peer Must be `SERVER_PEER`.
     * @param data Payload.
     * @param ch Delivery guarantee.
     */
    void send(ecs::net::PeerId peer, const ecs::net::Bytes& data, ecs::net::Channel ch) override;
    /**
     * @brief Sends a message to the server, the only peer of a client.
     * @param data Payload.
     * @param ch Delivery guarantee.
     */
    void broadcast(const ecs::net::Bytes& data, ecs::net::Channel ch) override;
    /**
     * @brief Takes the events received from the server.
     * @return Events in arrival order.
     */
    [[nodiscard]] std::vector<ecs::net::NetworkEvent> poll() override;
    /**
     * @brief Disconnects from the server.
     * @param peer Ignored.
     */
    void kick(ecs::net::PeerId peer) override;

  private:
    /** @brief Wrapped transport. */
    std::shared_ptr<ecs::net::INetworkClient> _client;
};

}  // namespace engine
