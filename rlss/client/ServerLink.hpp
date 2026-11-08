/**
 * @file ServerLink.hpp
 * @brief Connects to the server before the window opens, then hands the connection to the engine.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "net/INetworkClient.hpp"
#include "net/INetworkEventSource.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"

namespace rlss::client {

/**
 * @brief Adapts the client transport to the engine's `NetworkPlugin`, like `ecs::net::ClientConnection`, but first
 * hands out the events already read while connecting (the `Connected` event and what followed it), so the game
 * plugins see the whole session.
 *
 * It also drops the snapshots that do not come from the match the player is in (`setMatch()`): snapshots travel
 * unreliably, so a few of a match just left can arrive after the player went back to the menu or joined another
 * match. Applied, they would bring back the cars of the old match, or set a newer tick than the new match's.
 */
class ServerLink final : public ecs::net::INetworkService, public ecs::net::INetworkEventSource {
  public:
    /**
     * @brief Wraps a connected transport.
     * @param client Transport.
     * @param pending Events read by `connectToServer`, delivered on the first `poll()`.
     */
    ServerLink(std::shared_ptr<ecs::net::INetworkClient> client, std::vector<ecs::net::NetworkEvent> pending)
        : _client(std::move(client)), _pending(std::move(pending)) {}

    /**
     * @brief Sends a message to the server; dropped if `peer` is not `SERVER_PEER`.
     * @param peer Must be `SERVER_PEER`.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    void send(ecs::net::PeerId peer, const ecs::net::Bytes& data, ecs::net::Channel channel) override;

    /**
     * @brief Sends a message to the server, the only peer of a client.
     * @param data Payload.
     * @param channel Delivery guarantee.
     */
    void broadcast(const ecs::net::Bytes& data, ecs::net::Channel channel) override;

    /**
     * @brief Disconnects from the server.
     * @param peer Ignored.
     */
    void kick(ecs::net::PeerId peer) override;

    /**
     * @brief Takes the pending events, then the ones received from the server.
     * @return Events in arrival order.
     */
    [[nodiscard]] std::vector<ecs::net::NetworkEvent> poll() override;

    /**
     * @brief Sets the match whose snapshots are let through.
     * @param matchId Match joined, or nothing in the menu (no snapshot at all).
     */
    void setMatch(std::optional<std::uint16_t> matchId) noexcept { _match = matchId; }

  private:
    /** @brief Wrapped transport. */
    std::shared_ptr<ecs::net::INetworkClient> _client;
    /** @brief Events read before the engine started. */
    std::vector<ecs::net::NetworkEvent> _pending;
    /** @brief Match whose snapshots are let through, if any. */
    std::optional<std::uint16_t> _match;
};

/**
 * @brief Connects to the server and waits for the result, so that no window opens if the server cannot be reached.
 * @param client Transport, not started yet.
 * @param host Server name or address.
 * @param port Server port.
 * @param timeout Longest wait.
 * @return The events read since the connection succeeded (starting with `Connected`), or nothing if it failed;
 *         `client->disconnectReason()` then says why.
 */
[[nodiscard]] std::optional<std::vector<ecs::net::NetworkEvent>> connectToServer(ecs::net::INetworkClient& client,
                                                                                 const std::string& host,
                                                                                 std::uint16_t port,
                                                                                 std::chrono::milliseconds timeout);

/**
 * @brief Tells the player why the connection failed or ended.
 * @param reason Reason given by the transport.
 * @return Human-readable text.
 */
[[nodiscard]] const char* describe(ecs::net::DisconnectReason reason) noexcept;

}  // namespace rlss::client
