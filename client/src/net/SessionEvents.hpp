/**
 * @file SessionEvents.hpp
 * @brief Events between the client's screens and its session with the server.
 *
 * Screens publish the requests; `RTypeNetworkPlugin` handles them and publishes the results. Neither knows the other.
 */

#pragma once

#include <cstdint>
#include <string>

#include "protocol/Messages.hpp"

namespace client {

// --- Requests (published by the game, handled by RTypeNetworkPlugin) ------------------------------------------------

/** @brief Connect to a server; ignored while a session exists. */
struct ConnectRequest {
    /** @brief Host name or address. */
    std::string host;
    /** @brief TCP port. */
    std::uint16_t port = 0;
};

/** @brief Close the session (leaves the room first). */
struct DisconnectRequest {};

/** @brief Ask the server for the list of rooms again. */
struct RefreshRoomsRequest {};

/** @brief Create a room and enter it; only in the lobby. */
struct CreateRoomRequest {};

/** @brief Enter a room; only in the lobby. */
struct JoinRoomRequest {
    /** @brief Room to join. */
    std::uint32_t roomId = 0;
};

/** @brief Set the ready state in the room; only while waiting in a room. */
struct SetReadyRequest {
    /** @brief New ready state. */
    bool ready = false;
};

/** @brief Leave the room or the game, and go back to the lobby. */
struct LeaveRoomRequest {};

// --- Results (published by RTypeNetworkPlugin) -----------------------------------------------------------------------

/** @brief The server accepted the session (`Welcome`): the client is in the lobby. */
struct SessionConnected {};

/** @brief The session ended without being asked to: connection refused, failed or lost. */
struct SessionClosed {
    /** @brief `true` if the connection never got past the handshake. */
    bool duringConnection = false;
    /** @brief What to tell the player. */
    std::string reason;
};

/** @brief A new list of rooms arrived. */
struct RoomListUpdated {
    /** @brief The list. */
    rtype::protocol::LobbyList list;
};

/** @brief The room the client is in changed (players, ready states); the first one means it entered the room. */
struct RoomUpdated {
    /** @brief The room. */
    rtype::protocol::LobbyUpdate room;
};

/** @brief The client left its room or its game on request, and is back in the lobby. */
struct RoomLeft {};

/** @brief A player left the room or the game. */
struct PlayerLeftRoom {
    /** @brief Network id of the player. */
    std::uint32_t clientId = 0;
};

/** @brief The game has started and UDP is open. */
struct GameStarted {
    /** @brief Index of the ship this client controls. */
    std::uint8_t playerIndex = 0;
    /** @brief Number of players. */
    std::uint8_t playerCount = 0;
};

/** @brief A level starts. */
struct LevelStarted {
    /** @brief The level. */
    rtype::protocol::LevelStart level;
};

/** @brief A level was cleared. */
struct LevelCompleted {
    /** @brief The result of the level. */
    rtype::protocol::LevelComplete result;
};

/** @brief The scores of the room changed. */
struct ScoresUpdated {
    /** @brief Total score and score per player. */
    rtype::protocol::ScoreUpdate scores;
};

/** @brief The game is over; the client is back in the lobby, UDP closed and replicated entities gone. */
struct GameEnded {
    /** @brief Why, and the scores. */
    rtype::protocol::GameOver result;
};

}  // namespace client
