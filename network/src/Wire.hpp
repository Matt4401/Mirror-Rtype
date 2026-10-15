#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>

#include "Network/INetworkManager.hpp"

namespace rtype::net::wire {

enum class MessageKind : std::uint8_t {
    Data = 0,
    Welcome = 1,
    UdpOpen = 2,
    UdpClose = 3,
};

inline constexpr std::size_t TCP_HEADER_SIZE = 5;
inline constexpr std::uint32_t MAX_TCP_PAYLOAD = 64 * 1024;

inline constexpr std::size_t UDP_HEADER_SIZE = 8;
inline constexpr std::size_t MAX_DATAGRAM = 1200;
inline constexpr std::size_t MAX_UDP_PAYLOAD = MAX_DATAGRAM - UDP_HEADER_SIZE;

inline constexpr auto UDP_TICK = std::chrono::milliseconds(250);
inline constexpr auto KEEPALIVE_INTERVAL = std::chrono::seconds(1);
inline constexpr auto UDP_TIMEOUT = std::chrono::seconds(5);

inline void writeU32(const std::span<std::uint8_t, 4> out, const std::uint32_t value) {
    out[0] = value & 0xFF;
    out[1] = (value >> 8) & 0xFF;
    out[2] = (value >> 16) & 0xFF;
    out[3] = (value >> 24) & 0xFF;
}

inline std::uint32_t readU32(const std::span<const std::uint8_t, 4> in) {
    return in[0] | (in[1] << 8) | (in[2] << 16) | (static_cast<std::uint32_t>(in[3]) << 24);
}

inline bool isSendable(const Bytes& data, const Channel channel) {
    if (channel == Channel::Reliable) {
        if (data.size() <= MAX_TCP_PAYLOAD) {
            return true;
        }
        std::cerr << "[network] reliable payload of " << data.size() << " bytes refused (max " << MAX_TCP_PAYLOAD
                  << ")\n";
        return false;
    }
    if (!data.empty() && data.size() <= MAX_UDP_PAYLOAD) {
        return true;
    }
    std::cerr << "[network] unreliable payload of " << data.size() << " bytes refused (1-" << MAX_UDP_PAYLOAD << ")\n";
    return false;
}

}  // namespace rtype::net::wire
