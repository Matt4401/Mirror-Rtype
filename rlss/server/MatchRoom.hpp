/**
 * @file MatchRoom.hpp
 * @brief One RLSS match on the server: its own engine, its own Wren VM, its own thread.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "core/Engine.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/PeerGroupConnection.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::server {

/** @brief What the menu shows about a match, refreshed after each frame of the match. */
struct MatchSummary {
    /** @brief Score, phase and clock. */
    protocol::MatchStatus status;
    /** @brief Players in the blue team. */
    std::uint8_t bluePlayers = 0;
    /** @brief Players in the orange team. */
    std::uint8_t orangePlayers = 0;
};

/**
 * @brief One match: an `engine::Engine` running the RLSS simulation for the players of this match only.
 *
 * Each match has its own ECS world, event bus, clock and Wren VM (so the `Simulation` registry of the scripts only
 * holds its own cars and ball), and its network goes through an `ecs::net::PeerGroupConnection`: its snapshots and
 * statuses only reach its players.
 *
 * The `MatchServer` owns the transport and the player list: `addPlayer()` / `removePlayer()` push a `Connected` /
 * `Disconnected` event to the match, which spawns or removes the car on its own thread (`RosterPlugin`), and
 * `deliver()` hands over what a player sent. After `runInBackground()`, the engine is only touched by the match's
 * thread; the other thread only reads `summary()`, which is protected by a mutex.
 */
class MatchRoom {
  public:
    /**
     * @brief Creates and initializes the engine of a new match. Its clock starts with the first `advance()`.
     * @param transport Transport shared by every match; must outlive the room.
     * @param id Match id, written in the snapshots.
     * @param scriptRoot Directory the Wren scripts are read from; empty for the working directory.
     * @throws std::runtime_error If the engine cannot start (Wren bindings refused).
     */
    MatchRoom(ecs::net::INetworkServer& transport, std::uint16_t id, const std::filesystem::path& scriptRoot = {});
    /** @brief Stops and joins the thread of the match, if it has one, then shuts its engine down. */
    ~MatchRoom();

    /** @brief Deleted copy constructor: the thread of the match holds a pointer to it. */
    MatchRoom(const MatchRoom&) = delete;
    /** @brief Deleted copy assignment operator. */
    MatchRoom& operator=(const MatchRoom&) = delete;
    /** @brief Deleted move constructor. */
    MatchRoom(MatchRoom&&) = delete;
    /** @brief Deleted move assignment operator. */
    MatchRoom& operator=(MatchRoom&&) = delete;

    /** @brief Match id. */
    [[nodiscard]] std::uint16_t id() const noexcept { return _id; }

    /**
     * @brief Adds a player: from now on, the match reaches it, and spawns its car on its next frame.
     * @param peer Player.
     * @return False if the player is already in the match or the match is full.
     */
    bool addPlayer(ecs::net::PeerId peer);

    /**
     * @brief Removes a player: its car is removed on the next frame, and the match no longer reaches it.
     * @param peer Player.
     * @return False if the player was not in the match.
     */
    bool removePlayer(ecs::net::PeerId peer);

    /**
     * @brief Hands over a message a player of the match sent (inputs): it is decoded on the match's next frame.
     * @param peer Sender; ignored if not in the match.
     * @param data Untrusted payload.
     * @param channel Channel it arrived on.
     */
    void deliver(ecs::net::PeerId peer, const ecs::net::Bytes& data, ecs::net::Channel channel);

    /** @brief Runs one frame of the match (the simulation steps due, then the snapshots), then refreshes the summary.
     */
    void advance();

    /**
     * @brief Calls `advance()` in a loop on a new thread, until the room is destroyed. Call it once.
     *
     * An error thrown by the simulation stops this match only: it is logged and the match is reported as ended, so its
     * players get the end screen and go back to the menu.
     */
    void runInBackground();

    /** @brief Score, clock, phase and teams after the last frame. Thread-safe. */
    [[nodiscard]] MatchSummary summary() const;

    /** @brief True once the match is over (or stopped by an error). Thread-safe. */
    [[nodiscard]] bool hasEnded() const { return summary().status.phase == protocol::MatchPhase::Ended; }

    /** @brief Players in the match, as added by `addPlayer()`. */
    [[nodiscard]] std::size_t playerCount() const noexcept { return _players.size(); }

    /** @brief True if no other player can join. */
    [[nodiscard]] bool isFull() const noexcept { return _players.size() >= protocol::MAX_PLAYERS_PER_MATCH; }

  private:
    /** @brief Reports the match as ended after an error. */
    void fail();

    /** @brief Match id. */
    std::uint16_t _id;
    /** @brief Network of this match: what its engine receives and the only peers it can reach. */
    std::shared_ptr<ecs::net::PeerGroupConnection> _connection;
    /** @brief Engine of this match. */
    engine::Engine _engine;
    /** @brief Players, in join order; only used by the thread of the `MatchServer`. */
    std::vector<ecs::net::PeerId> _players;
    /** @brief Protects `_summary`. Mutable: `summary()` only reads. */
    mutable std::mutex _summaryMutex;
    /** @brief What the menu shows about the match. */
    MatchSummary _summary;
    /** @brief Set by the destructor to stop `_thread`. */
    std::atomic<bool> _stopping{false};
    /** @brief Runs the match once `runInBackground()` was called. */
    std::thread _thread;
};

}  // namespace rlss::server
