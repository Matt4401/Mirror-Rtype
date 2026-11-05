#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "Logger.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkConfig.hpp"
#include "net/asio/LogTag.hpp"

// Byte layout of the transport framing. Private to engine_network_asio: see docs/protocol/transport.md.
namespace ecs::net::detail::wire {

/** @brief Kind of a TCP frame, in its 5th byte. Only `Data` frames reach the game. */
enum class MessageKind : std::uint8_t {
    /** @brief Game message (both directions). */
    Data = 0,
    /** @brief Server → client: `[PeerId u32][token u32]`, the handshake was accepted. */
    Welcome = 1,
    /** @brief Client → server: the client starts using UDP. */
    UdpOpen = 2,
    /** @brief Client → server: the client stops using UDP. */
    UdpClose = 3,
    /** @brief Client → server, first frame: `[protocolId u32]`. */
    Handshake = 4,
};

/** @brief TCP frame header: `[payload length u32][kind u8]`. */
inline constexpr std::size_t TCP_HEADER_SIZE = 5;
/** @brief Size of the `Welcome` payload: `[PeerId u32][token u32]`. */
inline constexpr std::size_t WELCOME_SIZE = 8;
/** @brief Size of the `Handshake` payload: `[protocolId u32]`. */
inline constexpr std::size_t HANDSHAKE_SIZE = 4;

/** @brief Writes a little-endian u32. */
inline void writeU32(const std::span<std::uint8_t, 4> out, const std::uint32_t value) {
    out[0] = value & 0xFF;
    out[1] = (value >> 8) & 0xFF;
    out[2] = (value >> 16) & 0xFF;
    out[3] = (value >> 24) & 0xFF;
}

/** @brief Reads a little-endian u32. */
inline std::uint32_t readU32(const std::span<const std::uint8_t, 4> in) {
    return in[0] | (in[1] << 8) | (in[2] << 16) | (static_cast<std::uint32_t>(in[3]) << 24);
}

/**
 * @brief Checks a payload before queuing it, and logs why it is refused.
 * @param data Payload.
 * @param channel Channel it would be sent on.
 * @param maxReliablePayload `NetworkConfig::maxReliablePayload`.
 * @return True if it fits the channel.
 */
inline bool isSendable(const Bytes& data, const Channel channel, const std::uint32_t maxReliablePayload) {
    if (channel == Channel::Reliable) {
        if (data.size() <= maxReliablePayload) {
            return true;
        }
        rtype::log::error(LOG_TAG, "reliable payload of ", data.size(), " bytes refused (max ", maxReliablePayload,
                          ')');
        return false;
    }
    if (!data.empty() && data.size() <= MAX_UNRELIABLE_PAYLOAD) {
        return true;
    }
    rtype::log::error(LOG_TAG, "unreliable payload of ", data.size(), " bytes refused (1-", MAX_UNRELIABLE_PAYLOAD,
                      ')');
    return false;
}

}  // namespace ecs::net::detail::wire
