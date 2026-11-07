/**
 * @file ServerConnection.hpp
 * @brief Adapts the server transport to the engine's `NetworkPlugin` when one engine owns every peer.
 */

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "net/INetworkEventSource.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/INetworkService.hpp"

namespace rlss::server {

/**
 * @brief Both the event source and the network service of the server's single engine: every call goes straight to
 * the transport. (`ecs::net::PeerGroupConnection` is for servers running several engines on one transport.)
 */
class ServerConnection final : public ecs::net::INetworkService, public ecs::net::INetworkEventSource {
  public:
    /**
     * @brief Wraps a server transport.
     * @param server Transport, shared with `main` which starts and stops it.
     */
    explicit ServerConnection(std::shared_ptr<ecs::net::INetworkServer> server) : _server(std::move(server)) {}

    /**
     * @brief Sends a message to one peer.
     * @param peer Destination peer.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    void send(ecs::net::PeerId peer, const ecs::net::Bytes& data, ecs::net::Channel channel) override;

    /**
     * @brief Sends a message to every connected peer.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    void broadcast(const ecs::net::Bytes& data, ecs::net::Channel channel) override;

    /**
     * @brief Closes a peer's connection.
     * @param peer Peer to disconnect.
     */
    void kick(ecs::net::PeerId peer) override;

    /**
     * @brief Takes every event received since the last call.
     * @return Events in arrival order.
     */
    [[nodiscard]] std::vector<ecs::net::NetworkEvent> poll() override;

  private:
    /** @brief Wrapped transport. */
    std::shared_ptr<ecs::net::INetworkServer> _server;
};

}  // namespace rlss::server
