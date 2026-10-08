#include "NetworkClient.hpp"

#include <future>
#include <iostream>

namespace rtype::net::client {
NetworkClient::NetworkClient() : _work(asio::make_work_guard(_io)), _tcpSocket(_io), _udpSocket(_io), _tcpHeader() {}

NetworkClient::~NetworkClient() { NetworkClient::stop(); }

void NetworkClient::readTcpHeader() {
    asio::async_read(_tcpSocket, asio::buffer(_tcpHeader), [this](const std::error_code ec, std::size_t) {
        if (ec) {
            onTcpError(ec);
            return;
        }

        const std::uint32_t size = _tcpHeader[0] | (_tcpHeader[1] << 8) | (_tcpHeader[2] << 16) |
                                   (static_cast<std::uint32_t>(_tcpHeader[3]) << 24);

        if (size == 0 || size > MAX_TCP_MESSAGE) {
            onTcpError(asio::error::message_size);
            return;
        }
        readTcpBody(size);
    });
}

void NetworkClient::readTcpBody(const std::uint32_t size) {
    _tcpBody.resize(size);
    asio::async_read(_tcpSocket, asio::buffer(_tcpBody), [this](const std::error_code ec, std::size_t) {
        if (ec) {
            onTcpError(ec);
            return;
        }
        _events.push({.type = EventType::Data, .client = 0, .data = std::move(_tcpBody), .channel = Channel::Reliable});
        _tcpBody.clear();
        readTcpHeader();
    });
}

void NetworkClient::onTcpError(const std::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        return;
    }
    _events.push({.type = EventType::Disconnected, .client = 0, .data = {}, .channel = Channel::Reliable});
}

void NetworkClient::receiveUdp() {
    _udpSocket.async_receive_from(asio::buffer(_udpBuffer), _udpFrom,
                                  [this](const std::error_code ec, const std::size_t size) {
                                      if (ec == asio::error::operation_aborted) {
                                          return;
                                      }
                                      if (!ec && _udpFrom == _serverUdp && size > 0) {
                                          _events.push({.type = EventType::Data,
                                                        .client = 0,
                                                        .data = Bytes(_udpBuffer.begin(), _udpBuffer.begin() + size),
                                                        .channel = Channel::Unreliable});
                                      }
                                      receiveUdp();
                                  });
}

bool NetworkClient::connect(const std::string& host, const std::uint16_t port) {
    try {
        asio::ip::tcp::resolver resolver(_io);
        const auto endpoints = resolver.resolve(host, std::to_string(port));

        asio::connect(_tcpSocket, endpoints);
    } catch (asio::system_error& e) {
        std::cerr << "[network] cannot connect to " << host << ':' << port << ": " << e.what() << '\n';
        std::error_code ignored;
        _tcpSocket.close(ignored);
        return false;
    }
    readTcpHeader();
    _running = true;
    _thread = std::thread([this] { _io.run(); });
    return true;
}

bool NetworkClient::openUdp(const std::uint16_t port) {
    if (!_running.load()) {
        return false;
    }

    auto open = [this, port] {
        try {
            _serverUdp = {_tcpSocket.remote_endpoint().address(), port};
            _udpSocket = asio::ip::udp::socket(_io, {_serverUdp.protocol(), 0});
        } catch (const asio::system_error& e) {
            std::cerr << "[network] cannot open UDP to port " << port << ": " << e.what() << '\n';
            return false;
        }
        receiveUdp();
        return true;
    };
    std::future<bool> opened = asio::post(_io, asio::use_future(open));
    return opened.get();
}

void NetworkClient::closeUdp() {
    if (!_running.load()) {
        return;
    }

    asio::post(_io, [this] {
        std::error_code ignored;
        _udpSocket.close(ignored);
    });
}

void NetworkClient::stop() {
    if (!_running.exchange(false)) {
        return;
    }

    asio::post(_io, [this] {
        std::error_code ignored = asio::error::operation_aborted;
        _udpSocket.close(ignored);
        _tcpSocket.close(ignored);
    });

    _work.reset();
    if (_thread.joinable()) {
        _thread.join();
    }
}

}  // namespace rtype::net::client
