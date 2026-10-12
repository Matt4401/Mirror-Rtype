#pragma once

#include <array>
#include <asio/ip/tcp.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <system_error>

#include "Network/INetworkManager.hpp"
#include "Wire.hpp"

namespace rtype::net::session {

// Bytes waiting to be written to one peer. Above this, the peer is not reading anymore: it is disconnected.
inline constexpr std::size_t MAX_TCP_QUEUE_BYTES = std::size_t{1024} * 1024;

// Must be created with std::make_shared, and only used from the network thread.

class TcpSession : public std::enable_shared_from_this<TcpSession> {
  public:
    using MessageHandler = std::function<void(ClientId, wire::MessageKind, Bytes)>;
    using CloseHandler = std::function<void(ClientId)>;

    TcpSession(asio::ip::tcp::socket socket, ClientId id, MessageHandler onMessage, CloseHandler onClose);

    void start();
    void send(wire::MessageKind kind, const Bytes& payload = {});
    void close();

  private:
    void readHeader();
    void readBody(std::uint32_t size);
    void deliver(Bytes payload);
    void write();
    void onError(std::error_code ec);
    void closeNow();

    asio::ip::tcp::socket _socket;
    ClientId _id;
    MessageHandler _onMessage;
    CloseHandler _onClose;

    std::array<std::uint8_t, wire::TCP_HEADER_SIZE> _header{};
    Bytes _body;
    std::deque<Bytes> _writeQueue;
    std::size_t _queuedBytes = 0;
    bool _closing = false;
};

}  // namespace rtype::net::session
