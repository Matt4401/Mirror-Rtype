/**
 * @file MatchServer.hpp
 * @brief The RLSS server: the menu (match list, create, join, leave) and the matches being played.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "MatchRoom.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::server {

/**
 * @brief Owns the transport and routes every peer: to the menu, or to the match it plays in.
 *
 * - A peer that connects is in the menu. It sends its name (`SetName`), kept for every match it joins: the server
 *   hands it to the match right after the player, and again if the player renames while playing. Every `LIST_INTERVAL`,
 * the players in the menu get the `MatchList` (the matches not over yet, with their teams, score and clock) if it
 * changed; a player who arrives in the menu gets it at once.
 * - `CreateMatch` starts a new `MatchRoom` (at most `protocol::MAX_MATCHES`) and puts the player in it; `JoinMatch`
 *   puts the player in a match of the list, if it is not full nor over. Both are answered with a `JoinResult`.
 * - Once in a match, what the player sends goes to the match, except `LeaveMatch`, which takes them back to the menu.
 *   A match without players is destroyed, ended or not; a player who disconnects leaves their match.
 *
 * Each match runs on its own thread (unless `threaded` is false: then `update()` advances them, for the tests). Every
 * message from a client is untrusted: malformed ones are dropped.
 */
class MatchServer {
  public:
    /** @brief Time between two match lists sent to the players in the menu. */
    static constexpr std::chrono::milliseconds LIST_INTERVAL{500};

    /**
     * @brief Creates a server with no match.
     * @param transport Started transport; must outlive the server.
     * @param threaded True to run each match on its own thread; false to run them in `update()`.
     * @param scriptRoot Directory the matches read their Wren scripts from; empty for the working directory.
     */
    explicit MatchServer(std::shared_ptr<ecs::net::INetworkServer> transport, bool threaded = true,
                         std::filesystem::path scriptRoot = {});

    /**
     * @brief Handles the events received since the last call, then sends the match list if it is due.
     *
     * Not threaded, also runs one frame of every match.
     */
    void update();

    /** @brief Number of matches hosted, ended or not. */
    [[nodiscard]] std::size_t matchCount() const noexcept { return _rooms.size(); }

    /**
     * @brief Match a peer plays in.
     * @param peer Peer.
     * @return Its match id, or nothing if the peer is in the menu (or unknown).
     */
    [[nodiscard]] std::optional<std::uint16_t> matchOf(ecs::net::PeerId peer) const;

    /** @brief The list the players in the menu get: the matches not over yet, by id. */
    [[nodiscard]] protocol::MatchList matchList() const;

  private:
    /** @brief A peer finished the handshake: it is in the menu. */
    void onConnected(ecs::net::PeerId peer);
    /** @brief A peer is gone: it leaves its match, if any. */
    void onDisconnected(ecs::net::PeerId peer);
    /** @brief A peer sent a message: menu requests are handled here, the rest goes to its match. */
    void onData(const ecs::net::NetworkEvent& event);

    /** @brief Starts a new match and puts the peer in it. */
    void create(ecs::net::PeerId peer);
    /** @brief Puts the peer in a match of the list. */
    void join(ecs::net::PeerId peer, std::uint16_t matchId);
    /** @brief Hands the peer's name, if it sent one, to the match it plays in, as if the peer had sent it there. */
    void forwardName(ecs::net::PeerId peer);
    /** @brief Takes the peer out of its match, back to the menu; destroys the match if it is now empty. */
    void leave(ecs::net::PeerId peer);

    /** @brief Answers a `CreateMatch` / `JoinMatch`. */
    void sendResult(ecs::net::PeerId peer, protocol::JoinStatus status, std::uint16_t matchId = 0);
    /** @brief Sends the current match list to one peer. */
    void sendList(ecs::net::PeerId peer);
    /** @brief Sends the match list to every peer in the menu if it changed since the last time. */
    void broadcastList();
    /** @brief A match id not in use (never 0). */
    [[nodiscard]] std::uint16_t nextMatchId();

    /** @brief Shared transport. */
    std::shared_ptr<ecs::net::INetworkServer> _transport;
    /** @brief True if each match runs on its own thread. */
    bool _threaded;
    /** @brief Directory the matches read their Wren scripts from. */
    std::filesystem::path _scriptRoot;
    /** @brief Matches, by id: the list is sorted by id. */
    std::map<std::uint16_t, std::unique_ptr<MatchRoom>> _rooms;
    /** @brief Match of each peer in a match. */
    std::unordered_map<ecs::net::PeerId, std::uint16_t> _matchOf;
    /** @brief Every connected peer. */
    std::unordered_set<ecs::net::PeerId> _online;
    /** @brief Name of each peer that sent one (`protocol::isValidName`). */
    std::unordered_map<ecs::net::PeerId, std::string> _names;
    /** @brief Id tried first for the next match. */
    std::uint16_t _nextId = 1;
    /** @brief When the match list was last sent to the menu. */
    std::chrono::steady_clock::time_point _lastList;
    /** @brief Last match list sent to the menu. */
    std::optional<protocol::MatchList> _lastSent;
};

}  // namespace rlss::server
