#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rtype::protocol {

/**
 * @brief Protocol version sent in `Hello`; bumped on every breaking change of `rtype.proto`.
 *
 * Version 2: `PlayerInput` carries the game's action ids instead of a fixed button bitmask, and `EntityState`
 * positions are floats in world units instead of int16 pixels.
 * Version 3: a `WorldState` can be one part of a snapshot, so a client must not remove the entities it does not
 * contain.
 */
inline constexpr std::uint32_t PROTOCOL_VERSION = 3;

/**
 * @brief Value of `ecs::net::NetworkConfig::protocolId` for R-Type: the bytes `RTYP` on the wire (little-endian u32).
 *
 * It identifies the game, not its version: a peer announcing another id is refused during the transport handshake,
 * before `Hello` is even sent. `PROTOCOL_VERSION` is what changes with the format of the messages.
 */
inline constexpr std::uint32_t TRANSPORT_PROTOCOL_ID = 0x50595452;
/** @brief TCP and UDP port the server listens on when none is given. */
inline constexpr std::uint16_t DEFAULT_PORT = 4242;
/** @brief Maximum number of players in a room (player slots, scores). */
inline constexpr std::size_t MAX_PLAYERS = 4;
/** @brief Maximum number of rooms in one `LobbyList`. */
inline constexpr std::size_t MAX_ROOMS = 256;
/** @brief Maximum size of a chat message, in UTF-8 bytes. */
inline constexpr std::size_t MAX_CHAT_LENGTH = 256;
/** @brief Most levels in a game. */
inline constexpr std::size_t MAX_LEVELS = 64;
/** @brief Longest level name, in bytes. */
inline constexpr std::size_t MAX_LEVEL_NAME_LENGTH = 64;
/** @brief Largest encoded message accepted by `decode()`: the reliable channel's maximum payload (64 KiB). */
inline constexpr std::size_t MAX_MESSAGE_SIZE = std::size_t{64} * 1024;
/** @brief Largest encoded message that fits in one UDP datagram (1200 bytes minus the 8-byte library header). */
inline constexpr std::size_t MAX_UNRELIABLE_SIZE = 1192;

/** @brief Entities that always fit in one `WorldState` datagram: 32 bytes each at most, plus its header. */
inline constexpr std::size_t MAX_ENTITIES_PER_PART = 36;
/** @brief Maximum number of parts of one snapshot. */
inline constexpr std::uint32_t MAX_SNAPSHOT_PARTS = 64;

/** @brief Maximum number of button actions held in one `PlayerInput`. */
inline constexpr std::size_t MAX_INPUT_BUTTONS = 16;
/** @brief Maximum number of axis actions in one `PlayerInput`. */
inline constexpr std::size_t MAX_INPUT_AXES = 8;

/**
 * @brief Id of an input action: the FNV-1a hash of its name, the same value as the engine's `input::ActionId`
 * (`input::makeActionId("shoot")`).
 *
 * The protocol does not depend on the engine: it carries the ids without knowing what they mean, so any game can
 * send its own actions.
 */
using ActionId = std::uint32_t;

/** @brief C -> S: first message of the client, starts version negotiation. */
struct Hello {
    /** @brief Protocol version of the client; the server closes the connection if it differs. */
    std::uint32_t version = PROTOCOL_VERSION;
    bool operator==(const Hello&) const = default;
};

/** @brief S -> C: answer to `Hello`, gives the UDP port to open. */
struct Welcome {
    /** @brief UDP port of the server (1-65535). */
    std::uint16_t udpPort = 0;
    bool operator==(const Welcome&) const = default;
};

/** @brief C -> S: asks for the list of rooms. */
struct LobbyListRequest {
    bool operator==(const LobbyListRequest&) const = default;
};

/** @brief State of a room in the lobby. */
enum class RoomState : std::uint8_t {
    Waiting = 0,
    InGame = 1,
};

/** @brief One room of a `LobbyList`. */
struct RoomEntry {
    /** @brief Room id, assigned by the server. */
    std::uint32_t roomId = 0;
    /** @brief Current number of players (0 to `maxPlayers`). */
    std::uint8_t playerCount = 0;
    /** @brief Capacity of the room (1-`MAX_PLAYERS`). */
    std::uint8_t maxPlayers = 0;
    /** @brief Waiting for players or playing. */
    RoomState state = RoomState::Waiting;
    bool operator==(const RoomEntry&) const = default;
};

/** @brief S -> C: list of rooms, sent on request and whenever a room changes. */
struct LobbyList {
    /** @brief At most `MAX_ROOMS` rooms. */
    std::vector<RoomEntry> rooms;
    bool operator==(const LobbyList&) const = default;
};

/** @brief C -> S: creates a room and joins it. */
struct LobbyCreate {
    bool operator==(const LobbyCreate&) const = default;
};

/** @brief C -> S: joins a waiting room that is not full. */
struct LobbyJoin {
    /** @brief Room to join. */
    std::uint32_t roomId = 0;
    bool operator==(const LobbyJoin&) const = default;
};

/** @brief C -> S: sets the ready state of the client in its room. */
struct LobbyReady {
    /** @brief True when the client is ready to start. */
    bool ready = false;
    bool operator==(const LobbyReady&) const = default;
};

/** @brief One player of a `LobbyUpdate`. */
struct PlayerSlot {
    /** @brief Network id of the player. */
    std::uint32_t clientId = 0;
    /** @brief Ready state of the player. */
    bool ready = false;
    bool operator==(const PlayerSlot&) const = default;
};

/** @brief S -> C: players of a room, pushed when one joins, leaves or changes their ready state. */
struct LobbyUpdate {
    /** @brief Updated room. */
    std::uint32_t roomId = 0;
    /** @brief Players in join order, at most `MAX_PLAYERS`. */
    std::vector<PlayerSlot> slots;
    bool operator==(const LobbyUpdate&) const = default;
};

/** @brief S -> C: the game starts; the client opens UDP. */
struct GameStart {
    /** @brief Room that started. */
    std::uint32_t roomId = 0;
    /** @brief Ship controlled by this client (0 to `playerCount` - 1). */
    std::uint8_t playerIndex = 0;
    /** @brief Number of players in the game (1-`MAX_PLAYERS`). */
    std::uint8_t playerCount = 0;
    bool operator==(const GameStart&) const = default;
};

/** @brief Why a game ended. */
enum class GameOverReason : std::uint8_t {
    AllDead = 0,
    Victory = 1,
    ServerShutdown = 2,
};

/** @brief S -> C: the game ended; the client closes UDP and goes back to the lobby. */
struct GameOver {
    /** @brief Why the game ended. */
    GameOverReason reason = GameOverReason::AllDead;
    /** @brief Final cumulative score. */
    std::uint32_t score = 0;
    /** @brief Score per player slot, at most `MAX_PLAYERS`. */
    std::vector<std::uint32_t> playerScores;
    bool operator==(const GameOver&) const = default;
};

/** @brief S -> C: a level starts (the first one right after `GameStart`, the next ones after `LevelComplete`). */
struct LevelStart {
    /** @brief Index of the level, below `levelCount`. */
    std::uint8_t levelIndex = 0;
    /** @brief Number of levels of the game (1-`MAX_LEVELS`). */
    std::uint8_t levelCount = 1;
    /** @brief Display name, UTF-8, at most `MAX_LEVEL_NAME_LENGTH` bytes. */
    std::string name;
    bool operator==(const LevelStart&) const = default;
};

/** @brief S -> C: a level was cleared; the next `LevelStart` (or `GameOver` after the last level) follows. */
struct LevelComplete {
    /** @brief Index of the cleared level, below `MAX_LEVELS`. */
    std::uint8_t levelIndex = 0;
    /** @brief Cumulative score of the room so far. */
    std::uint32_t score = 0;
    /** @brief Score per player slot, at most `MAX_PLAYERS`. */
    std::vector<std::uint32_t> playerScores;
    bool operator==(const LevelComplete&) const = default;
};

/** @brief S -> C: a player left the room. */
struct PlayerLeft {
    /** @brief Network id of the player who left. */
    std::uint32_t clientId = 0;
    bool operator==(const PlayerLeft&) const = default;
};

/** @brief C -> S: leaves the current room (lobby or game) and goes back to the lobby; also sent before closing. */
struct Leave {
    bool operator==(const Leave&) const = default;
};

/** @brief Both ways: chat message. The server fills `clientId` before broadcasting it to the room. */
struct Chat {
    /** @brief Sender (ignored by the server on reception). */
    std::uint32_t clientId = 0;
    /** @brief UTF-8 text, 1 to `MAX_CHAT_LENGTH` bytes. */
    std::string text;
    bool operator==(const Chat&) const = default;
};

/** @brief Value of one axis action during a client tick. */
struct AxisValue {
    /** @brief Axis action. */
    ActionId action = 0;
    /** @brief Value of the axis, in [-1, 1]. */
    float value = 0.0F;
    bool operator==(const AxisValue&) const = default;
};

/**
 * @brief C -> S over UDP: state of the player's actions during one client tick.
 *
 * Built from the client's `input::InputState`. The actions are the game's own: the protocol only carries their ids.
 */
struct PlayerInput {
    /** @brief Increases with every input; late or duplicated inputs are dropped by the server. */
    std::uint32_t sequence = 0;
    /** @brief Button actions held down during this tick, at most `MAX_INPUT_BUTTONS`. */
    std::vector<ActionId> held;
    /** @brief Axis actions whose value is not 0, at most `MAX_INPUT_AXES`. */
    std::vector<AxisValue> axes;
    bool operator==(const PlayerInput&) const = default;
};

/**
 * @brief Kind of an entity in a `WorldState`: the FNV-1a hash of its prefab name (see `makeEntityKind()`).
 *
 * The protocol knows no entity kind: each game sends its own prefabs, and the client picks the sprite from the kind.
 */
using EntityKind = std::uint32_t;

/**
 * @brief Computes the kind of the entities spawned from a prefab: the 32-bit FNV-1a hash of its name.
 *
 * Same algorithm as the engine's `input::makeActionId()`, so that both sides compute the same value without sharing a
 * table. Usable at compile time: `constexpr auto MISSILE = makeEntityKind("missile");`.
 *
 * @param prefab Prefab name, e.g. "alien_scout".
 * @return Kind to put in `EntityState::kind`.
 */
constexpr EntityKind makeEntityKind(const std::string_view prefab) noexcept {
    constexpr std::uint32_t OFFSET_BASIS = 0x811C9DC5;
    constexpr std::uint32_t PRIME = 0x01000193;
    std::uint32_t hash = OFFSET_BASIS;
    for (const char c : prefab) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= PRIME;
    }
    return hash;
}

/** @brief State of one entity in a `WorldState`. */
struct EntityState {
    /** @brief Id of the entity, stable for its whole life. */
    std::uint32_t entityId = 0;
    /** @brief Kind of entity: hash of its prefab name, see `makeEntityKind()`. */
    EntityKind kind = 0;
    /** @brief Game-defined variant of the kind (R-Type: player index of a ship). */
    std::uint8_t subtype = 0;
    /** @brief X position in world units, same as `Transform::x`. */
    float x = 0.0F;
    /** @brief Y position in world units, same as `Transform::y`. */
    float y = 0.0F;
    /** @brief Health points; 0 means the entity died this tick (sent once). */
    std::uint8_t health = 0;
    /** @brief Game-defined bits (the protocol gives them no meaning). */
    std::uint8_t flags = 0;
    bool operator==(const EntityState&) const = default;
};

/** @brief S -> C over UDP: authoritative snapshot of a room, sent after each server step. */
struct WorldState {
    /** @brief Server step of this snapshot; older snapshots are dropped by the client. */
    std::uint32_t tick = 0;
    /** @brief Sequence of the last input of this client applied by the server, for reconciliation. */
    std::uint32_t lastProcessedInput = 0;
    /** @brief Entities of this part of the snapshot. */
    std::vector<EntityState> entities;
    /** @brief Index of this part, below `partCount`. */
    std::uint32_t partIndex = 0;
    /** @brief Number of parts of the snapshot, from 1 to `MAX_SNAPSHOT_PARTS`. */
    std::uint32_t partCount = 1;
    bool operator==(const WorldState&) const = default;
};

/** @brief Any message of the game protocol: one `Envelope` on the wire. */
using Message =
    std::variant<Hello, Welcome, LobbyListRequest, LobbyList, LobbyCreate, LobbyJoin, LobbyReady, LobbyUpdate,
                 GameStart, GameOver, PlayerLeft, Leave, Chat, PlayerInput, WorldState, LevelStart, LevelComplete>;

}  // namespace rtype::protocol
