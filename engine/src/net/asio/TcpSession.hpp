#pragma once

#include <array>
#include <asio/ip/tcp.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <system_error>

#include "net/INetworkManager.hpp"
#include "net/asio/Wire.hpp"

namespace ecs::net::detail {

/**
 * @brief One framed TCP connection: `[length u32][kind u8][payload]`.
 *
 * Must be created with `std::make_shared`, and only used from the network thread.
 */
class TcpSession : public std::enable_shared_from_this<TcpSession> {
  public:
    /** @brief Called for each complete frame received. */
    using MessageHandler = std::function<void(PeerId, wire::MessageKind, Bytes)>;
    /** @brief Called once, when the connection is closed (by either side). */
    using CloseHandler = std::function<void(PeerId)>;

    /** @brief Size limits of a session, taken from `NetworkConfig`. */
    struct Limits {
        /** @brief Largest payload sent or received; a peer announcing more is disconnected. */
        std::uint32_t maxPayload;
        /** @brief Bytes waiting to be written; above this, the peer is disconnected. */
        std::size_t maxQueueBytes;
    };

    /**
     * @brief Wraps a connected socket. Nothing is read before `start()`.
     * @param socket Connected socket.
     * @param id Peer id passed back to the handlers.
     * @param limits Size limits.
     * @param onMessage Frame handler.
     * @param onClose Close handler.
     */
    TcpSession(asio::ip::tcp::socket socket, PeerId id, Limits limits, MessageHandler onMessage, CloseHandler onClose);

    /** @brief Starts reading frames. */
    void start();
    /** @brief Queues a frame; closes the session if the peer does not read anymore. */
    void send(wire::MessageKind kind, const Bytes& payload = {});
    /** @brief Closes the connection; `onClose` is called later, from the network thread. */
    void close();

  private:
    void readHeader();
    void readBody(std::uint32_t size);
    void deliver(Bytes payload);
    void write();
    void onError(std::error_code ec);
    void closeNow();

    asio::ip::tcp::socket _socket;
    PeerId _id;
    Limits _limits;
    MessageHandler _onMessage;
    CloseHandler _onClose;

    std::array<std::uint8_t, wire::TCP_HEADER_SIZE> _header{};
    Bytes _body;
    std::deque<Bytes> _writeQueue;
    std::size_t _queuedBytes = 0;
    bool _closing = false;
};

}  // namespace ecs::net::detail
