#pragma once

#include <cstdint>
#include <vector>

namespace ecs::net {

/** @brief Identifier of a remote peer, given by the transport. 0 is never assigned. */
using PeerId = std::uint32_t;
/** @brief Raw message payload: the transport never reads it. */
using Bytes = std::vector<std::uint8_t>;

/** @brief Kind of a `NetworkEvent`. */
enum class EventType : std::uint8_t {
    /** @brief A peer finished the transport handshake: it can now send and receive. */
    Connected,
    /** @brief A peer is gone (closed, crashed, timed out, kicked). Never sent for a peer that was not `Connected`. */
    Disconnected,
    /** @brief A peer sent a game message. */
    Data,
};

/** @brief Delivery guarantee of a message. */
enum class Channel : std::uint8_t {
    /** @brief Ordered and guaranteed (TCP). */
    Reliable,
    /** @brief Fast, may be lost, duplicated or reordered (UDP). */
    Unreliable,
};

/** @brief Lifecycle state of a transport. */
enum class State : std::uint8_t {
    /** @brief Created, not started yet. */
    Idle,
    /** @brief Started: events can arrive. */
    Running,
    /** @brief Stopped: it cannot be restarted. */
    Stopped,
};

/** @brief Something that happened on the network, delivered by `INetworkManager::poll()`. */
struct NetworkEvent {
    /** @brief What happened. */
    EventType type;
    /** @brief Peer concerned. On the client, always 0 (the server). */
    PeerId peer;
    /** @brief Message payload, for `EventType::Data` only. */
    Bytes data;
    /** @brief Channel the message arrived on, for `EventType::Data` only. */
    Channel channel;
};

/**
 * @brief Common part of the client and server transports.
 *
 * The transport runs on its own thread; `poll()` hands its events to the game thread. Implementations are created by
 * `createServer()` / `createClient()` (see `NetworkFactory.hpp`), so the game never sees the socket library.
 */
class INetworkManager {
  public:
    /** @brief Default constructor. */
    INetworkManager() = default;
    /** @brief Deleted copy constructor: a transport owns sockets and a thread. */
    INetworkManager(const INetworkManager&) = delete;
    /** @brief Deleted copy assignment operator. */
    INetworkManager& operator=(const INetworkManager&) = delete;
    /** @brief Deleted move constructor: the network thread holds a pointer to the transport. */
    INetworkManager(INetworkManager&&) = delete;
    /** @brief Deleted move assignment operator. */
    INetworkManager& operator=(INetworkManager&&) = delete;

    /** @brief Virtual destructor; implementations stop their thread here. */
    virtual ~INetworkManager() = default;

    /**
     * @brief Takes every event received since the last call. Thread-safe, never blocks.
     * @return Events in arrival order.
     */
    virtual std::vector<NetworkEvent> poll() = 0;
    /** @brief Closes every connection and joins the network thread. Safe to call twice. */
    virtual void stop() = 0;
    /** @brief True between a successful start (`start()` / `connect()`) and `stop()` or a lost connection. */
    [[nodiscard]] virtual bool isRunning() const = 0;
};

}  // namespace ecs::net
