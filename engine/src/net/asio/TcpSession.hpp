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
    /** @brief Closes the connection once the queued frames are written; nothing is sent or delivered until then. */
    void closeAfterFlush();

  private:
    /** @brief Reads the header of the next frame. */
    void readHeader();
    /** @brief Reads the payload announced by the header. @param size Length of the payload. */
    void readBody(std::uint32_t size);
    /** @brief Hands a complete frame to `onMessage`, then reads the next one. @param payload Payload of the frame. */
    void deliver(Bytes payload);
    /** @brief Writes the oldest queued frame, then the next ones. */
    void write();
    /** @brief Closes the session after a read or write error. @param ec The error. */
    void onError(std::error_code ec);
    /** @brief Closes the socket and calls `onClose`. */
    void closeNow();

    /** @brief The connection. */
    asio::ip::tcp::socket _socket;
    /** @brief Peer id passed back to the handlers. */
    PeerId _id;
    /** @brief Size limits. */
    Limits _limits;
    /** @brief Frame handler. */
    MessageHandler _onMessage;
    /** @brief Close handler. */
    CloseHandler _onClose;

    /** @brief Header of the frame being read. */
    std::array<std::uint8_t, wire::TCP_HEADER_SIZE> _header{};
    /** @brief Payload of the frame being read. */
    Bytes _body;
    /** @brief Frames waiting to be written, oldest first. */
    std::deque<Bytes> _writeQueue;
    /** @brief Total size of `_writeQueue`. */
    std::size_t _queuedBytes = 0;
    /** @brief True once `close()` was called. */
    bool _closing = false;
    /** @brief True once `closeAfterFlush()` was called, until the queue is written. */
    bool _flushing = false;
};

}  // namespace ecs::net::detail
