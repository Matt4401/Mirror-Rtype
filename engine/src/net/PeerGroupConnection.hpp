#pragma once

#include <mutex>
#include <unordered_set>
#include <vector>

#include "INetworkEventSource.hpp"
#include "INetworkManager.hpp"
#include "INetworkServer.hpp"
#include "INetworkService.hpp"

namespace ecs::net {

/**
 * @brief Adapts a server transport to the `NetworkPlugin` of one engine, among several sharing that transport.
 *
 * A transport has a single event queue for every peer, so the code that owns it polls it once and routes each event
 * with `push()`. In the other direction, the engine can only reach the peers of its group: `broadcast()` does not
 * leak to the peers of another group, and `send()` / `kick()` ignore them.
 */
class PeerGroupConnection final : public INetworkService, public INetworkEventSource {
  public:
    /**
     * @brief Creates an empty group.
     * @param server Transport, which must outlive this connection.
     */
    explicit PeerGroupConnection(INetworkServer& server) : _server(&server) {}

    /**
     * @brief Adds a peer to the group.
     * @param peer Peer to add.
     */
    void addPeer(PeerId peer);
    /**
     * @brief Removes a peer from the group. Its events still queued are kept: they arrived while it was a member.
     * @param peer Peer to remove.
     */
    void removePeer(PeerId peer);

    /**
     * @brief Queues an event for the engine's next `poll()`; dropped if its peer is not in the group.
     *
     * The queue is only emptied by `poll()`: stop pushing when the engine does not run, or it grows without limit.
     *
     * @param event Event taken from the transport.
     */
    void push(NetworkEvent event);

    /**
     * @brief Takes the events pushed since the last call.
     * @return Events in arrival order.
     */
    [[nodiscard]] std::vector<NetworkEvent> poll() override;

    /**
     * @brief Sends a message to a peer of the group; dropped for any other peer.
     * @param peer Destination peer.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    void send(PeerId peer, const Bytes& data, Channel channel) override;
    /**
     * @brief Sends a message to every peer of the group.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    void broadcast(const Bytes& data, Channel channel) override;
    /**
     * @brief Closes the connection of a peer of the group; ignored for any other peer.
     * @param peer Peer to disconnect.
     */
    void kick(PeerId peer) override;

  private:
    /** @brief Shared transport. */
    INetworkServer* _server;
    /** @brief Protects `_peers` and `_events`: `INetworkService` is thread-safe. */
    std::mutex _mutex;
    /** @brief Peers of the group. */
    std::unordered_set<PeerId> _peers;
    /** @brief Events waiting for `poll()`. */
    std::vector<NetworkEvent> _events;
};

}  // namespace ecs::net
