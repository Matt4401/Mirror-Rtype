/**
 * @file Protocol.hpp
 * @brief RLSS game protocol: constants shared by server and client, and the message codec.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace rlss::protocol {

/** @brief Name of the game, shown in the window title. */
inline constexpr std::string_view GAME_TITLE = "RLSS";

/**
 * @brief Transport protocol id (`ecs::net::NetworkConfig::protocolId`): an R-Type client and an RLSS server refuse
 * each other during the handshake. Bump the low byte on every incompatible protocol change.
 */
inline constexpr std::uint32_t PROTOCOL_ID = 0x524C0005U;  // "RL", version 5

/** @brief Port used when none is given on the command line (TCP and UDP). */
inline constexpr std::uint16_t DEFAULT_PORT = 4250;

/** @brief Raw message bytes, as carried by the transport. */
using Bytes = std::vector<std::uint8_t>;

/** @brief First byte of every message: tells how to read the rest. */
enum class MessageType : std::uint8_t {
    /** @brief Server → client, reliable: state of the lobby. */
    LobbyState = 1,
    /** @brief Server → client, unreliable: one part of a world snapshot. */
    Snapshot = 2,
    /** @brief Client → server, unreliable: input of the local player for one step. */
    PlayerInput = 3,
    /** @brief Server → client, reliable: score and phase of the match. */
    MatchStatus = 4,
    /** @brief Server → client, reliable: which car is the receiver's. */
    Welcome = 5,
};

/** @brief Server → client: state of the lobby, sent to everyone when a player joins or leaves. */
struct LobbyState {
    /** @brief Number of clients currently connected to the server. */
    std::uint16_t players = 0;
};

/** @brief State of one replicated entity in a snapshot. */
struct EntityState {
    /** @brief Network id (never 0). */
    std::uint32_t id = 0;
    /** @brief Game-defined kind (`rlss::game::KIND_CAR`, `rlss::game::KIND_BALL`). */
    std::uint8_t kind = 0;
    /** @brief World position, in arena units. */
    float x = 0.0F;
    /** @brief World position, in arena units (y points down). */
    float y = 0.0F;
    /** @brief Rotation in radians, clockwise on screen. */
    float rotation = 0.0F;
    /** @brief True when the entity faces left (mirrored). */
    bool facingLeft = false;
    /** @brief Index in the player color palette. */
    std::uint8_t color = 0;
    /** @brief Car: boost left, from 0 to 100. */
    std::uint8_t boost = 0;
    /** @brief Car: boosting right now (shows the flame). */
    bool boosting = false;
};

/** @brief Most entities one snapshot part carries: keeps a part far below the 1192-byte unreliable payload limit. */
inline constexpr std::size_t MAX_ENTITIES_PER_PART = 48;

/** @brief Most parts one snapshot can be split into (same limit as the engine's `MAX_SNAPSHOT_PARTS`). */
inline constexpr std::uint8_t MAX_SNAPSHOT_PARTS = 64;

/** @brief Server → client: one part of the world state at a server step. */
struct Snapshot {
    /** @brief Server step. */
    std::uint32_t tick = 0;
    /** @brief Sequence of the last input of the receiving client applied by the server. */
    std::uint32_t lastProcessedInput = 0;
    /** @brief Index of this part, below `partCount`. */
    std::uint8_t partIndex = 0;
    /** @brief Number of parts of this snapshot, from 1 to `MAX_SNAPSHOT_PARTS`. */
    std::uint8_t partCount = 1;
    /** @brief Entities of this part, at most `MAX_ENTITIES_PER_PART`. */
    std::vector<EntityState> entities;
};

/** @brief Client → server: input of the local player for one step. */
struct PlayerInput {
    /** @brief Sequence number, increasing with every input. */
    std::uint32_t sequence = 0;
    /** @brief Horizontal stick / arrow keys, in [-1, 1]: drive on the ground, rotate in the air. */
    float steer = 0.0F;
    /** @brief Jump button held. */
    bool jump = false;
    /** @brief Boost button held. */
    bool boost = false;
};

/** @brief Phase of the match. */
enum class MatchPhase : std::uint8_t {
    /** @brief The ball is in play. */
    Playing = 0,
    /** @brief A goal was just scored: celebration before the kickoff. */
    Goal = 1,
    /** @brief Cars and ball are back in place, frozen during the countdown. */
    Kickoff = 2,
};

/** @brief Highest kickoff countdown value. */
inline constexpr std::uint8_t MAX_COUNTDOWN = 9;

/** @brief Server → client: score and phase, sent when they change and to every player who joins. */
struct MatchStatus {
    /** @brief Goals of the blue team (left side). */
    std::uint16_t blueScore = 0;
    /** @brief Goals of the orange team (right side). */
    std::uint16_t orangeScore = 0;
    /** @brief Current phase. */
    MatchPhase phase = MatchPhase::Playing;
    /** @brief Kickoff countdown shown to the players (3, 2, 1), 0 outside the kickoff. */
    std::uint8_t countdown = 0;

    /** @brief Field-wise equality. */
    bool operator==(const MatchStatus&) const = default;
};

/** @brief Server → client: sent to a player once their car exists, so that the camera can follow it. */
struct Welcome {
    /** @brief Network id of the player's car (never 0), as found in the snapshots. */
    std::uint32_t carId = 0;
};

/** @brief Any decoded message. */
using Message = std::variant<LobbyState, Snapshot, PlayerInput, MatchStatus, Welcome>;

/**
 * @brief Encodes a lobby state: `[type u8][players u16]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const LobbyState& message);

/**
 * @brief Encodes a snapshot part: `[type u8][tick u32][lastInput u32][partIndex u8][partCount u8][count u8]`, then
 * per entity `[id u32][kind u8][x f32][y f32][rotation f32][flags u8][color u8][boost u8]`; flags: bit 0 facing left,
 * bit 1 boosting.
 * @param message Message to encode; entities past `MAX_ENTITIES_PER_PART` are not written.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const Snapshot& message);

/**
 * @brief Encodes a player input: `[type u8][sequence u32][steer i8][buttons u8]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const PlayerInput& message);

/**
 * @brief Encodes the match status: `[type u8][blueScore u16][orangeScore u16][phase u8][countdown u8]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const MatchStatus& message);

/**
 * @brief Encodes a welcome: `[type u8][carId u32]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const Welcome& message);

/**
 * @brief Decodes a message received from the network. Multi-byte integers are big-endian, floats are IEEE 754
 * binary32. The bytes are untrusted: an unknown type, a wrong size, a non-finite float or an out-of-range field is
 * rejected, never read past the end.
 * @param data Raw payload.
 * @return The message, or nothing if the payload is malformed.
 */
[[nodiscard]] std::optional<Message> decode(std::span<const std::uint8_t> data);

}  // namespace rlss::protocol
