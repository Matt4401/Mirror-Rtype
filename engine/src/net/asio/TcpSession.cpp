#include "net/asio/TcpSession.hpp"

#include <algorithm>
#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/post.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <system_error>
#include <tuple>
#include <utility>

#include "log/Logger.hpp"
#include "net/INetworkManager.hpp"
#include "net/asio/LogTag.hpp"
#include "net/asio/TransportCounters.hpp"
#include "net/asio/Wire.hpp"

namespace ecs::net::detail {

namespace {

/** @brief Time a peer has to close its side after `closeAfterFlush()`: it holds a socket, so it must stay short. */
constexpr auto LINGER_TIMEOUT = std::chrono::milliseconds(500);

}  // namespace

TcpSession::TcpSession(asio::ip::tcp::socket socket, const PeerId id, const Limits limits, TransportCounters& counters,
                       MessageHandler onMessage, CloseHandler onClose)
    : _socket(std::move(socket)),
      _id(id),
      _limits(limits),
      _counters(&counters),
      _onMessage(std::move(onMessage)),
      _onClose(std::move(onClose)),
      _lingerTimer(_socket.get_executor()) {}

void TcpSession::start() { readHeader(); }

// misc-no-recursion reports readHeader -> readBody -> deliver -> readHeader (and write -> write) as recursion. It is
// not: each function only *starts* an async operation and returns. Asio never runs a completion handler inside the
// initiating call; the handler runs later from io_context::run(), on a fresh stack. The stack never grows.
// NOLINTBEGIN(misc-no-recursion)
void TcpSession::readHeader() {
    asio::async_read(_socket, asio::buffer(_header),
                     [self = shared_from_this()](const std::error_code ec, std::size_t) {
                         if (ec) {
                             self->onError(ec);
                             return;
                         }

                         const std::uint32_t size = wire::readU32(std::span(self->_header).first<4>());
                         if (size > self->_limits.maxPayload) {
                             self->onError(asio::error::message_size);
                             return;
                         }
                         if (size == 0) {
                             self->deliver({});
                             return;
                         }
                         self->readBody(size);
                     });
}

void TcpSession::readBody(const std::uint32_t size) {
    _body.resize(size);
    asio::async_read(_socket, asio::buffer(_body), [self = shared_from_this()](const std::error_code ec, std::size_t) {
        if (ec) {
            self->onError(ec);
            return;
        }
        self->deliver(std::move(self->_body));
    });
}

void TcpSession::deliver(Bytes payload) {
    _counters->onReceived(wire::TCP_HEADER_SIZE + payload.size());
    if (_closing) {
        return;
    }
    if (!_flushing) {
        _onMessage(_id, static_cast<wire::MessageKind>(_header[4]), std::move(payload));
    }
    // The owner may have closed the session (invalid kind, kick...): stop reading in that case.
    if (!_closing) {
        readHeader();
    }
}

void TcpSession::send(const wire::MessageKind kind, const Bytes& payload) {
    if (_closing || _flushing) {
        return;
    }
    if (payload.size() > _limits.maxPayload) {
        engine::log::error(LOG_TAG, "reliable payload of ", payload.size(), " bytes refused (max ", _limits.maxPayload,
                           ')');
        return;
    }

    Bytes frame(wire::TCP_HEADER_SIZE + payload.size());
    wire::writeU32(std::span(frame).first<4>(), static_cast<std::uint32_t>(payload.size()));
    frame[4] = static_cast<std::uint8_t>(kind);
    std::ranges::copy(payload, frame.begin() + wire::TCP_HEADER_SIZE);

    if (_queuedBytes + frame.size() > _limits.maxQueueBytes) {
        engine::log::warning(LOG_TAG, "peer ", _id, " does not read its TCP socket anymore: disconnected");
        close();
        return;
    }

    const bool writing = !_writeQueue.empty();
    _queuedBytes += frame.size();
    _writeQueue.push_back(std::move(frame));
    if (!writing) {
        write();
    }
}

void TcpSession::write() {
    asio::async_write(_socket, asio::buffer(_writeQueue.front()),
                      [self = shared_from_this()](const std::error_code ec, std::size_t) {
                          if (ec) {
                              self->onError(ec);
                              return;
                          }
                          self->_counters->onSent(self->_writeQueue.front().size());
                          self->_queuedBytes -= self->_writeQueue.front().size();
                          self->_writeQueue.pop_front();
                          if (!self->_writeQueue.empty()) {
                              self->write();
                          } else if (self->_flushing) {
                              self->linger();
                          }
                      });
}

// NOLINTEND(misc-no-recursion)

void TcpSession::onError(const std::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        return;
    }
    close();
}

void TcpSession::close() {
    if (_closing) {
        return;
    }
    _closing = true;
    asio::post(_socket.get_executor(), [self = shared_from_this()] { self->closeNow(); });
}

void TcpSession::closeAfterFlush() {
    if (_closing || _flushing) {
        return;
    }
    _flushing = true;
    if (_writeQueue.empty()) {
        linger();
    }
}

void TcpSession::linger() {
    // Closing a socket with unread data resets the connection, and on Windows a reset destroys what the peer has not
    // read yet: the frame just written would be lost. So only end this side, and keep reading (deliver() discards the
    // frames) until the peer closes, which the read reports as an error.
    std::error_code ignored;
    std::ignore = _socket.shutdown(asio::ip::tcp::socket::shutdown_send, ignored);
    _lingerTimer.expires_after(LINGER_TIMEOUT);
    _lingerTimer.async_wait([self = shared_from_this()](const std::error_code ec) {
        if (!ec) {
            self->close();
        }
    });
}

void TcpSession::closeNow() {
    _lingerTimer.cancel();
    if (!_socket.is_open()) {
        return;
    }
    std::error_code ignored;
    std::ignore = _socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    std::ignore = _socket.close(ignored);
    if (_onClose) {
        _onClose(_id);
    }
}

}  // namespace ecs::net::detail
