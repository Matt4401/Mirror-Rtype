#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace rtype::protocol {

/** @brief Protocol version sent in `Hello`; bumped on every breaking change of `rtype.proto`. */
inline constexpr std::uint32_t PROTOCOL_VERSION = 1;

/** @brief Maximum number of players in a room (player slots, scores). */
inline constexpr std::size_t MAX_PLAYERS = 4;
/** @brief Maximum number of rooms in one `LobbyList`. */
inline constexpr std::size_t MAX_ROOMS = 256;
/** @brief Maximum size of a chat message, in UTF-8 bytes. */
inline constexpr std::size_t MAX_CHAT_LENGTH = 256;
/** @brief Largest encoded message accepted by `decode()`: the reliable channel's maximum payload (64 KiB). */
inline constexpr std::size_t MAX_MESSAGE_SIZE = std::size_t{64} * 1024;
/** @brief Largest encoded message that fits in one UDP datagram (1200 bytes minus the 8-byte library header). */
inline constexpr std::size_t MAX_UNRELIABLE_SIZE = 1192;

/** @brief `PlayerInput::buttons` bit: move up. */
inline constexpr std::uint8_t BUTTON_UP = 0x01;
/** @brief `PlayerInput::buttons` bit: move down. */
inline constexpr std::uint8_t BUTTON_DOWN = 0x02;
/** @brief `PlayerInput::buttons` bit: move left. */
inline constexpr std::uint8_t BUTTON_LEFT = 0x04;
/** @brief `PlayerInput::buttons` bit: move right. */
inline constexpr std::uint8_t BUTTON_RIGHT = 0x08;
/** @brief `PlayerInput::buttons` bit: fire. */
inline constexpr std::uint8_t BUTTON_SHOOT = 0x10;

/** @brief Every defined button bit (`BUTTON_UP` to `BUTTON_SHOOT`); the others are reserved and cleared by `decode()`.
 */
inline constexpr auto BUTTONS_MASK = static_cast<std::uint8_t>(0x1F);

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
    /** @brief Current number of players (0-`MAX_PLAYERS`). */
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

/** @brief S -> C: a player left the room. */
struct PlayerLeft {
    /** @brief Network id of the player who left. */
    std::uint32_t clientId = 0;
    bool operator==(const PlayerLeft&) const = default;
};

/** @brief C -> S: graceful disconnection, sent before closing the connection. */
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

/** @brief C -> S over UDP: controls held by the player during one client tick. */
struct PlayerInput {
    /** @brief Increases with every input; late or duplicated inputs are dropped by the server. */
    std::uint32_t sequence = 0;
    /** @brief Bitmask of `BUTTON_*`. */
    std::uint8_t buttons = 0;
    bool operator==(const PlayerInput&) const = default;
};

/** @brief Kind of entity in a `WorldState`. */
enum class EntityType : std::uint8_t {
    Player = 0,
    Enemy = 1,
    PlayerMissile = 2,
    EnemyMissile = 3,
    Powerup = 4,
};

/** @brief `EntityState::flags` bit: the entity is alive. */
inline constexpr std::uint8_t ENTITY_ALIVE = 0x01;
/** @brief `EntityState::flags` bit: the entity is shooting. */
inline constexpr std::uint8_t ENTITY_SHOOTING = 0x02;

/** @brief State of one entity in a `WorldState`. */
struct EntityState {
    /** @brief Id of the entity, stable for its whole life. */
    std::uint32_t entityId = 0;
    /** @brief Kind of entity. */
    EntityType type = EntityType::Player;
    /** @brief Type-specific variant (player index, enemy model...). */
    std::uint8_t subtype = 0;
    /** @brief X position in pixels (0 = left edge). */
    std::int16_t x = 0;
    /** @brief Y position in pixels (0 = top edge). */
    std::int16_t y = 0;
    /** @brief Health points; 0 means the entity died this tick (sent once). */
    std::uint8_t health = 0;
    /** @brief Bitmask of `ENTITY_*` flags. */
    std::uint8_t flags = 0;
    bool operator==(const EntityState&) const = default;
};

/** @brief S -> C over UDP: authoritative snapshot of a room, sent after each server step. */
struct WorldState {
    /** @brief Server step of this snapshot; older snapshots are dropped by the client. */
    std::uint32_t tick = 0;
    /** @brief Sequence of the last input of this client applied by the server, for reconciliation. */
    std::uint32_t lastProcessedInput = 0;
    /** @brief Entities of the room. */
    std::vector<EntityState> entities;
    bool operator==(const WorldState&) const = default;
};

/** @brief Any message of the game protocol: one `Envelope` on the wire. */
using Message = std::variant<Hello, Welcome, LobbyListRequest, LobbyList, LobbyCreate, LobbyJoin, LobbyReady,
                             LobbyUpdate, GameStart, GameOver, PlayerLeft, Leave, Chat, PlayerInput, WorldState>;

}  // namespace rtype::protocol
