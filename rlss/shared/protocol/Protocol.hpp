/**
 * @file Protocol.hpp
 * @brief RLSS game protocol: constants shared by server and client, and the message codec.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
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
inline constexpr std::uint32_t PROTOCOL_ID = 0x524C0009U;  // "RL", version 9

/** @brief Port used when none is given on the command line (TCP and UDP). */
inline constexpr std::uint16_t DEFAULT_PORT = 4250;

/** @brief Raw message bytes, as carried by the transport. */
using Bytes = std::vector<std::uint8_t>;

/** @brief Most players in one match: two teams of `PLAYERS_PER_TEAM`. */
inline constexpr std::uint8_t MAX_PLAYERS_PER_MATCH = 4;

/** @brief Most players in one team. */
inline constexpr std::uint8_t PLAYERS_PER_TEAM = 2;

/** @brief Most matches a server hosts at once, and so the most entries of a `MatchList`. */
inline constexpr std::uint8_t MAX_MATCHES = 16;

/** @brief Highest match clock value, in seconds (99:59). */
inline constexpr std::uint16_t MAX_CLOCK = 5999;

/** @brief Team of a player: even player slots are blue, odd ones orange. */
enum class Team : std::uint8_t {
    /** @brief Left side, defends the left goal. */
    Blue = 0,
    /** @brief Right side, defends the right goal. */
    Orange = 1,
};

/** @brief First byte of every message: tells how to read the rest. */
enum class MessageType : std::uint8_t {
    /** @brief Server → client, reliable: the matches being played, sent to the players in the menu. */
    MatchList = 1,
    /** @brief Server → client, unreliable: one part of a world snapshot. */
    Snapshot = 2,
    /** @brief Client → server, unreliable: input of the local player for one step. */
    PlayerInput = 3,
    /** @brief Server → client, reliable: score, clock and phase of the match. */
    MatchStatus = 4,
    /** @brief Server → client, reliable: which car is the receiver's. */
    Welcome = 5,
    /** @brief Client → server, reliable: creates a match and joins it. */
    CreateMatch = 6,
    /** @brief Client → server, reliable: joins a match of the list. */
    JoinMatch = 7,
    /** @brief Client → server, reliable: leaves the current match, back to the menu. */
    LeaveMatch = 8,
    /** @brief Server → client, reliable: answer to `CreateMatch` / `JoinMatch`. */
    JoinResult = 9,
    /** @brief Client → server, reliable: the player's name, sent once connected (again to change it). */
    SetName = 10,
    /** @brief Server → client, reliable: the name of each car of the match, sent when the players change. */
    PlayerNames = 11,
};

/** @brief Longest player name, in characters. */
inline constexpr std::size_t MAX_NAME_LENGTH = 12;

/**
 * @brief Whether a character may be part of a player name: upper-case letters, digits, space, `-` and `.` (all drawn by
 * the block font of the client).
 * @param character Character.
 * @return True if allowed.
 */
[[nodiscard]] bool isNameCharacter(char character) noexcept;

/**
 * @brief Whether a player name is valid: 1 to `MAX_NAME_LENGTH` allowed characters (`isNameCharacter`), not starting
 * nor ending with a space.
 * @param name Name.
 * @return True if valid.
 */
[[nodiscard]] bool isValidName(std::string_view name) noexcept;

/** @brief One match of a `MatchList`. */
struct MatchInfo {
    /** @brief Match id (never 0), to put in `JoinMatch`. */
    std::uint16_t id = 0;
    /** @brief Players in the blue team, at most `PLAYERS_PER_TEAM`. */
    std::uint8_t bluePlayers = 0;
    /** @brief Players in the orange team, at most `PLAYERS_PER_TEAM`. */
    std::uint8_t orangePlayers = 0;
    /** @brief Goals of the blue team. */
    std::uint16_t blueScore = 0;
    /** @brief Goals of the orange team. */
    std::uint16_t orangeScore = 0;
    /** @brief Seconds left, or seconds of overtime played; at most `MAX_CLOCK`. */
    std::uint16_t clock = 0;
    /** @brief True in overtime: the next goal wins. */
    bool overtime = false;

    /** @brief Field-wise equality. */
    bool operator==(const MatchInfo&) const = default;
};

/** @brief Server → client: the matches still being played, sent to the players in the menu. */
struct MatchList {
    /** @brief Clients connected to the server, in a match or not. */
    std::uint16_t online = 0;
    /** @brief Matches, at most `MAX_MATCHES`. */
    std::vector<MatchInfo> matches;

    /** @brief Field-wise equality. */
    bool operator==(const MatchList&) const = default;
};

/** @brief Highest `SoundCue::count` and `SoundCue::value`: each travels on 4 bits. */
inline constexpr std::uint8_t SOUND_CUE_MAX = 15;

/**
 * @brief Something an entity did that makes a sound (a jump, a hit, ...), carried by every snapshot.
 *
 * `count` wraps around: the client plays the sound when it changes, so a lost snapshot only delays it.
 */
struct SoundCue {
    /** @brief How many times it happened, modulo 16. */
    std::uint8_t count = 0;
    /** @brief Detail of the last time, 0 to `SOUND_CUE_MAX` (a jump kind, a strength). */
    std::uint8_t value = 0;

    /** @brief Field-wise equality. */
    bool operator==(const SoundCue&) const = default;
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
    /** @brief Car: index in the player color palette. Ball: color of the last shot (0 none, 1 red, 2 purple, 3 gold).
     */
    std::uint8_t color = 0;
    /** @brief Car: boost left, from 0 to 100. */
    std::uint8_t boost = 0;
    /** @brief Car: boosting right now (shows the flame). */
    bool boosting = false;
    /** @brief Car: flip regained on the ball, not used yet (the car flashes white). */
    bool flipReset = false;
    /** @brief Car: jumps (value: jump kind). Ball: hits by a car (value: strength). */
    SoundCue firstCue;
    /** @brief Car: impacts against the arena (value: strength). Ball: bounces off the arena (value: strength). */
    SoundCue secondCue;
};

/** @brief Most entities one snapshot part carries: keeps a part far below the 1192-byte unreliable payload limit. */
inline constexpr std::size_t MAX_ENTITIES_PER_PART = 48;

/** @brief Most parts one snapshot can be split into (same limit as the engine's `MAX_SNAPSHOT_PARTS`). */
inline constexpr std::uint8_t MAX_SNAPSHOT_PARTS = 64;

/** @brief Server → client: one part of the world state at a server step. */
struct Snapshot {
    /** @brief Match the snapshot comes from: a client drops the late ones of a match it left. */
    std::uint16_t matchId = 0;
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
    /** @brief Time is up: the match is over, everything stays frozen. */
    Ended = 3,
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
    /** @brief Seconds left, or seconds of overtime played; at most `MAX_CLOCK`. */
    std::uint16_t clock = 0;
    /** @brief True in overtime: the next goal wins. */
    bool overtime = false;

    /** @brief Field-wise equality. */
    bool operator==(const MatchStatus&) const = default;
};

/** @brief Server → client: sent to a player once their car exists, so that the camera can follow it. */
struct Welcome {
    /** @brief Network id of the player's car (never 0), as found in the snapshots. */
    std::uint32_t carId = 0;
    /** @brief Team of the player. */
    Team team = Team::Blue;
};

/** @brief Client → server: creates a new match and joins it. */
struct CreateMatch {};

/** @brief Client → server: joins a match of the list. */
struct JoinMatch {
    /** @brief Id read from the `MatchList`. */
    std::uint16_t matchId = 0;
};

/** @brief Client → server: leaves the current match. */
struct LeaveMatch {};

/** @brief Outcome of a `CreateMatch` or a `JoinMatch`. */
enum class JoinStatus : std::uint8_t {
    /** @brief The player is in the match: its `Welcome` and snapshots follow. */
    Joined = 0,
    /** @brief The match already has `MAX_PLAYERS_PER_MATCH` players. */
    Full = 1,
    /** @brief No such match, or it is over. */
    NotFound = 2,
    /** @brief The server already hosts `MAX_MATCHES` matches. */
    ServerFull = 3,
};

/** @brief Server → client: answer to a `CreateMatch` or a `JoinMatch`. */
struct JoinResult {
    /** @brief Outcome. */
    JoinStatus status = JoinStatus::Joined;
    /** @brief Match joined (never 0 when `Joined`), 0 otherwise. */
    std::uint16_t matchId = 0;
};

/** @brief Client → server: the player's name (`isValidName`), shown above their car. */
struct SetName {
    /** @brief Name. */
    std::string name;

    /** @brief Field-wise equality. */
    bool operator==(const SetName&) const = default;
};

/** @brief The name of the player driving a car. */
struct PlayerName {
    /** @brief Network id of the car (never 0), as found in the snapshots. */
    std::uint32_t carId = 0;
    /** @brief Name of its player (`isValidName`). */
    std::string name;

    /** @brief Field-wise equality. */
    bool operator==(const PlayerName&) const = default;
};

/** @brief Server → client: the names of every player of the match, sent when one joins, leaves or renames. */
struct PlayerNames {
    /** @brief One per car, at most `MAX_PLAYERS_PER_MATCH`. */
    std::vector<PlayerName> players;

    /** @brief Field-wise equality. */
    bool operator==(const PlayerNames&) const = default;
};

/** @brief Any decoded message. */
using Message = std::variant<MatchList, Snapshot, PlayerInput, MatchStatus, Welcome, CreateMatch, JoinMatch, LeaveMatch,
                             JoinResult, SetName, PlayerNames>;

/**
 * @brief Encodes a match list: `[type u8][online u16][count u8]`, then per match
 * `[id u16][bluePlayers u8][orangePlayers u8][blueScore u16][orangeScore u16][clock u16][flags u8]`; flags: bit 0
 * overtime.
 * @param message Message to encode; matches past `MAX_MATCHES` are not written.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const MatchList& message);

/**
 * @brief Encodes a snapshot part: `[type u8][matchId u16][tick u32][lastInput u32][partIndex u8][partCount u8]
 * [count u8]`, then
 * per entity `[id u32][kind u8][x f32][y f32][rotation f32][flags u8][color u8][boost u8][cueCounts u8][cueValues u8]`;
 * flags: bit 0 facing left, bit 1 boosting, bit 2 flip reset; cue bytes: first cue in the low 4 bits, second in the
 * high 4 bits.
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
 * @brief Encodes the match status: `[type u8][blueScore u16][orangeScore u16][phase u8][countdown u8][clock u16]
 * [flags u8]`; flags: bit 0 overtime.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const MatchStatus& message);

/**
 * @brief Encodes a welcome: `[type u8][carId u32][team u8]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const Welcome& message);

/**
 * @brief Encodes a match creation request: `[type u8]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const CreateMatch& message);

/**
 * @brief Encodes a join request: `[type u8][matchId u16]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const JoinMatch& message);

/**
 * @brief Encodes a leave request: `[type u8]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const LeaveMatch& message);

/**
 * @brief Encodes a join result: `[type u8][status u8][matchId u16]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const JoinResult& message);

/**
 * @brief Encodes a name: `[type u8][length u8][name]`.
 * @param message Message to encode; the name is cut to `MAX_NAME_LENGTH` characters.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const SetName& message);

/**
 * @brief Encodes the names of a match: `[type u8][count u8]`, then per player `[carId u32][length u8][name]`.
 * @param message Message to encode; players past `MAX_PLAYERS_PER_MATCH` are not written, names are cut to
 * `MAX_NAME_LENGTH` characters.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const PlayerNames& message);

/**
 * @brief Decodes a message received from the network. Multi-byte integers are big-endian, floats are IEEE 754
 * binary32. The bytes are untrusted: an unknown type, a wrong size, a non-finite float or an out-of-range field is
 * rejected, never read past the end.
 * @param data Raw payload.
 * @return The message, or nothing if the payload is malformed.
 */
[[nodiscard]] std::optional<Message> decode(std::span<const std::uint8_t> data);

/**
 * @brief Reads the match id of a snapshot without decoding the rest, to drop the snapshots of another match early.
 * @param data Raw payload (untrusted).
 * @return The match id, or nothing if the payload is not a snapshot (or too short to hold one).
 */
[[nodiscard]] std::optional<std::uint16_t> snapshotMatch(std::span<const std::uint8_t> data);

}  // namespace rlss::protocol
