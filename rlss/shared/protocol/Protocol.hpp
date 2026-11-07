/**
 * @file Protocol.hpp
 * @brief RLSS game protocol: constants shared by server and client, and the message codec.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace rlss::protocol {

/** @brief Name of the game, shown in the window title and on screen. */
inline constexpr std::string_view GAME_TITLE = "RLSS";

/**
 * @brief Transport protocol id (`ecs::net::NetworkConfig::protocolId`): an R-Type client and a RLSS server
 * refuse each other during the handshake. Bump the low byte on every incompatible protocol change.
 */
inline constexpr std::uint32_t PROTOCOL_ID = 0x524C0001U;  // "RL", version 1

/** @brief Port used when none is given on the command line (TCP and UDP). */
inline constexpr std::uint16_t DEFAULT_PORT = 4250;

/** @brief Raw message bytes, as carried by the transport. */
using Bytes = std::vector<std::uint8_t>;

/** @brief First byte of every message: tells how to read the rest. */
enum class MessageType : std::uint8_t {
    /** @brief Server → client: state of the lobby. */
    LobbyState = 1,
};

/** @brief Server → client: state of the lobby, sent to everyone when a player joins or leaves. */
struct LobbyState {
    /** @brief Number of clients currently connected to the server. */
    std::uint16_t players = 0;
};

/** @brief Any decoded message. */
using Message = std::variant<LobbyState>;

/**
 * @brief Encodes a lobby state: `[type u8][players u16 big-endian]`.
 * @param message Message to encode.
 * @return Wire bytes.
 */
[[nodiscard]] Bytes encode(const LobbyState& message);

/**
 * @brief Decodes a message received from the network. The bytes are untrusted: any unknown type or wrong size is
 * rejected, never read past the end.
 * @param data Raw payload.
 * @return The message, or nothing if the payload is malformed.
 */
[[nodiscard]] std::optional<Message> decode(std::span<const std::uint8_t> data);

}  // namespace rlss::protocol
