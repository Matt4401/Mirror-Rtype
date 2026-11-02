#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "protocol/Messages.hpp"

/**
 * @brief Game-layer protocol: turns `Message`s into bytes and back.
 *
 * The wire format is Protocol Buffers (`common/proto/rtype.proto`): each payload is one serialized `Envelope`.
 * Protobuf types never leave `Protocol.cpp`; game code only sees the plain structs of `Messages.hpp`.
 */
namespace rtype::protocol {

/**
 * @brief Serializes a message into one payload, ready for `send()`.
 *
 * The caller keeps values in their documented ranges; unreliable messages must stay within `MAX_UNRELIABLE_SIZE`.
 *
 * @param message Message to encode.
 * @return The encoded bytes (never empty).
 */
[[nodiscard]] std::vector<std::uint8_t> encode(const Message& message);

/**
 * @brief Parses an untrusted payload.
 *
 * Rejects payloads larger than `MAX_MESSAGE_SIZE`, malformed protobuf, envelopes without a known message, unknown
 * enum values, out-of-range numbers and lists longer than their documented maximum. Reserved bits of
 * `PlayerInput::buttons` are cleared.
 *
 * @param bytes Payload received from the network.
 * @return The message, or `std::nullopt` if the payload is invalid: drop it.
 */
[[nodiscard]] std::optional<Message> decode(std::span<const std::uint8_t> bytes);

}  // namespace rtype::protocol
