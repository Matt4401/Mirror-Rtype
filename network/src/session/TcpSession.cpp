#include "TcpSession.hpp"

#include <algorithm>
#include <utility>

namespace rtype::net::session {

TcpSession::TcpSession(asio::ip::tcp::socket socket, const ClientId id, queue::EventQueue& events, CloseHandler onClose)
    : _socket(std::move(socket)), _id(id), _events(events), _onClose(std::move(onClose)) {}

void TcpSession::start() { readHeader(); }

void TcpSession::readHeader() {
    asio::async_read(
        _socket, asio::buffer(_header), [self = shared_from_this()](const std::error_code ec, std::size_t) {
            if (ec) {
                self->onError(ec);
                return;
            }

            const std::uint32_t size = self->_header[0] | (self->_header[1] << 8) | (self->_header[2] << 16) |
                                       (static_cast<std::uint32_t>(self->_header[3]) << 24);

            if (size == 0 || size > MAX_TCP_MESSAGE) {
                self->onError(asio::error::message_size);
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
        self->_events.push({.type = EventType::Data,
                            .client = self->_id,
                            .data = std::move(self->_body),
                            .channel = Channel::Reliable});
        self->_body.clear();
        self->readHeader();
    });
}

void TcpSession::send(const Bytes& data) {
    if (!_socket.is_open() || data.empty() || data.size() > MAX_TCP_MESSAGE) {
        return;
    }

    const auto size = static_cast<std::uint32_t>(data.size());
    Bytes frame(_header.size() + data.size());
    frame[0] = size & 0xFF;
    frame[1] = (size >> 8) & 0xFF;
    frame[2] = (size >> 16) & 0xFF;
    frame[3] = (size >> 24) & 0xFF;
    std::ranges::copy(data, frame.begin() + _header.size());

    const bool writing = !_writeQueue.empty();
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
                          self->_writeQueue.pop_front();
                          if (!self->_writeQueue.empty()) {
                              self->write();
                          }
                      });
}

void TcpSession::onError(const std::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        return;
    }
    close();
}

void TcpSession::close() {
    if (!_socket.is_open()) {
        return;
    }
    const auto self = shared_from_this();

    std::error_code ignored;
    _socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    _socket.close(ignored);
    _writeQueue.clear();
    if (_onClose) {
        _onClose(_id);
    }
}

}  // namespace rtype::net::session
