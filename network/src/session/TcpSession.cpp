#include "TcpSession.hpp"

#include <algorithm>
#include <iostream>
#include <span>
#include <utility>

namespace rtype::net::session {

TcpSession::TcpSession(asio::ip::tcp::socket socket, const ClientId id, MessageHandler onMessage, CloseHandler onClose)
    : _socket(std::move(socket)), _id(id), _onMessage(std::move(onMessage)), _onClose(std::move(onClose)) {}

void TcpSession::start() { readHeader(); }

void TcpSession::readHeader() {
    asio::async_read(_socket, asio::buffer(_header),
                     [self = shared_from_this()](const std::error_code ec, std::size_t) {
                         if (ec) {
                             self->onError(ec);
                             return;
                         }

                         const std::uint32_t size = wire::readU32(std::span(self->_header).first<4>());
                         if (size > wire::MAX_TCP_PAYLOAD) {
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
    if (_closing) {
        return;
    }
    _onMessage(_id, static_cast<wire::MessageKind>(_header[4]), std::move(payload));
    // The owner may have closed the session (invalid kind, kick...): stop reading in that case.
    if (!_closing) {
        readHeader();
    }
}

void TcpSession::send(const wire::MessageKind kind, const Bytes& payload) {
    if (_closing) {
        return;
    }
    if (payload.size() > wire::MAX_TCP_PAYLOAD) {
        std::cerr << "[network] reliable payload of " << payload.size() << " bytes refused (max "
                  << wire::MAX_TCP_PAYLOAD << ")\n";
        return;
    }

    Bytes frame(wire::TCP_HEADER_SIZE + payload.size());
    wire::writeU32(std::span(frame).first<4>(), static_cast<std::uint32_t>(payload.size()));
    frame[4] = static_cast<std::uint8_t>(kind);
    std::ranges::copy(payload, frame.begin() + wire::TCP_HEADER_SIZE);

    if (_queuedBytes + frame.size() > MAX_TCP_QUEUE_BYTES) {
        std::cerr << "[network] client " << _id << " does not read its TCP socket anymore: disconnected\n";
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
                          self->_queuedBytes -= self->_writeQueue.front().size();
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
    if (_closing) {
        return;
    }
    _closing = true;
    asio::post(_socket.get_executor(), [self = shared_from_this()] { self->closeNow(); });
}

void TcpSession::closeNow() {
    if (!_socket.is_open()) {
        return;
    }
    std::error_code ignored;
    _socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    _socket.close(ignored);
    if (_onClose) {
        _onClose(_id);
    }
}

}  // namespace rtype::net::session
