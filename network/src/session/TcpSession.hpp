#pragma once

#include <array>
#include <asio.hpp>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>

#include "EventQueue.hpp"
#include "Network/INetworkManager.hpp"

namespace rtype::net::session {

static constexpr std::uint32_t MAX_TCP_MESSAGE = 64 * 1024;

// One TCP connection (used by both NetworkServer and NetworkClient): length-prefixed reads, queued writes, close.
// Must be created with std::make_shared, and only used from the network thread.
class TcpSession : public std::enable_shared_from_this<TcpSession> {
  public:
    using CloseHandler = std::function<void(ClientId)>;

    TcpSession(asio::ip::tcp::socket socket, ClientId id, queue::EventQueue& events, CloseHandler onClose);

    void start();
    void send(const Bytes& data);
    void close();

  private:
    void readHeader();
    void readBody(std::uint32_t size);
    void write();
    void onError(std::error_code ec);

    asio::ip::tcp::socket _socket;
    ClientId _id;
    queue::EventQueue& _events;
    CloseHandler _onClose;

    std::array<std::uint8_t, 4> _header{};
    Bytes _body;
    std::deque<Bytes> _writeQueue;
};

}  // namespace rtype::net::session
