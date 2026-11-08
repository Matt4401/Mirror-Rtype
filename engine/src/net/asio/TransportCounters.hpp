#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "net/INetworkManager.hpp"

namespace ecs::net::detail {

/** @brief Traffic of a transport: written by the network thread, read by the game thread through `stats()`. */
class TransportCounters {
  public:
    /** @brief A TCP frame or a datagram was sent. @param bytes Its size, transport header included. */
    void onSent(const std::size_t bytes) noexcept {
        _bytesSent.fetch_add(bytes, std::memory_order_relaxed);
        _packetsSent.fetch_add(1, std::memory_order_relaxed);
    }

    /** @brief A TCP frame or a datagram was received. @param bytes Its size, transport header included. */
    void onReceived(const std::size_t bytes) noexcept {
        _bytesReceived.fetch_add(bytes, std::memory_order_relaxed);
        _packetsReceived.fetch_add(1, std::memory_order_relaxed);
    }

    /** @brief Reads the counters. @return The traffic so far. */
    [[nodiscard]] TransportStats read() const noexcept {
        return {
            .bytesSent = _bytesSent.load(std::memory_order_relaxed),
            .bytesReceived = _bytesReceived.load(std::memory_order_relaxed),
            .packetsSent = _packetsSent.load(std::memory_order_relaxed),
            .packetsReceived = _packetsReceived.load(std::memory_order_relaxed),
        };
    }

  private:
    /** @brief Bytes sent so far. */
    std::atomic<std::uint64_t> _bytesSent{0};
    /** @brief Bytes received so far. */
    std::atomic<std::uint64_t> _bytesReceived{0};
    /** @brief TCP frames and datagrams sent so far. */
    std::atomic<std::uint64_t> _packetsSent{0};
    /** @brief TCP frames and datagrams received so far. */
    std::atomic<std::uint64_t> _packetsReceived{0};
};

}  // namespace ecs::net::detail
