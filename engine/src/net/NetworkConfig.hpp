#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace ecs::net {

/** @brief Largest UDP datagram sent or accepted, header included: stays below the usual MTU to avoid fragmentation. */
inline constexpr std::size_t MAX_DATAGRAM = 1200;
/** @brief Transport header of a client datagram: `[PeerId u32][token u32]`. */
inline constexpr std::size_t DATAGRAM_HEADER_SIZE = 8;
/** @brief Largest unreliable payload a game can send, in both directions. */
inline constexpr std::size_t MAX_UNRELIABLE_PAYLOAD = MAX_DATAGRAM - DATAGRAM_HEADER_SIZE;

/**
 * @brief Settings of a transport, chosen by the game.
 *
 * Every field has the default the engine was tuned with; a game only overrides what it needs, e.g.
 * `createServer({.protocolId = MY_GAME_ID, .maxPeers = 16})`. Server and client must use the same `protocolId`.
 */
struct NetworkConfig {
    /**
     * @brief Identifies the game protocol. A peer announcing another id is refused during the transport handshake,
     * before the game sees it: two different games built on the engine never talk to each other.
     */
    std::uint32_t protocolId = 0;
    /** @brief Server only: connections kept at the same time (handshaking ones included); others are closed. */
    std::size_t maxPeers = 64;
    /** @brief Period of the UDP housekeeping (keepalives, timeouts, bind resends). */
    std::chrono::milliseconds udpTick{250};
    /** @brief An empty datagram is sent when nothing was sent on UDP for this long. */
    std::chrono::milliseconds keepaliveInterval{1000};
    /** @brief While UDP is open, a peer silent on UDP for this long is disconnected. */
    std::chrono::milliseconds udpTimeout{5000};
    /** @brief Server only: a connection that has not completed the handshake within this delay is closed. */
    std::chrono::milliseconds handshakeTimeout{5000};
    /** @brief Largest reliable payload, sent or received; a peer announcing more is disconnected. */
    std::uint32_t maxReliablePayload = 64U * 1024U;
    /** @brief Bytes waiting to be written to one peer; above this, the peer is not reading and is disconnected. */
    std::size_t maxReliableQueueBytes = std::size_t{1024} * 1024U;
};

/**
 * @brief Checks that a configuration can run: non-zero durations and limits, keepalive shorter than the timeout.
 * @param config Configuration to check.
 * @return True if the transport can use it.
 */
[[nodiscard]] constexpr bool isValid(const NetworkConfig& config) noexcept {
    using std::chrono::milliseconds;
    return config.maxPeers > 0 && config.udpTick > milliseconds::zero() &&
           config.keepaliveInterval > milliseconds::zero() && config.keepaliveInterval < config.udpTimeout &&
           config.handshakeTimeout > milliseconds::zero() && config.maxReliablePayload > 0 &&
           config.maxReliableQueueBytes >= config.maxReliablePayload;
}

}  // namespace ecs::net
