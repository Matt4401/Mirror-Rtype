#include "NetworkClient.hpp"

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

void NetworkClient::readTcpBody(std::uint32_t size) {
    _tcpBody.resize(size);
    asio::async_read(_tcpSocket, asio::buffer(_tcpBody), [this](const std::error_code ec, std::size_t) {
        if (ec) {
            onTcpError(ec);
            return;
        }
        _events.push({EventType::Data, 0, std::move(_tcpBody), Channel::Reliable});
        _tcpBody.clear();
        readTcpHeader();
    });
}

void NetworkClient::onTcpError(std::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        return;
    }
    _events.push({EventType::Disconnected, 0, {}, Channel::Reliable});
}

void NetworkClient::receiveUdp() {
    _udpSocket.async_receive_from(asio::buffer(_udpBuffer), _udpFrom, [this](std::error_code ec, std::size_t size) {
        if (ec == asio::error::operation_aborted) {
            return;
        }
        if (!ec && _udpFrom == _serverUdp && size > 0) {
            _events.push(
                {EventType::Data, 0, Bytes(_udpBuffer.begin(), _udpBuffer.begin() + size), Channel::Unreliable});
        }
        receiveUdp();
    });
}

bool NetworkClient::connect(const std::string& host, const std::uint16_t port) {
    try {
        asio::ip::tcp::resolver resolver(_io);
        const auto endpoints = resolver.resolve(host, std::to_string(port));

        asio::connect(_tcpSocket, endpoints);
        _serverUdp = asio::ip::udp::endpoint(_tcpSocket.remote_endpoint().address(), port);
        _udpSocket.open(asio::ip::udp::v4());
        _udpSocket.bind({asio::ip::udp::v4(), 0});
    } catch (asio::system_error& e) {
        std::cerr << "[network] cannot connect to " << host << ':' << port << ": " << e.what() << '\n';
        std::error_code ignored;
        _tcpSocket.close(ignored);
        _udpSocket.close(ignored);
        return false;
    }
    readTcpHeader();
    receiveUdp();
    _thread = std::thread([this] { _io.run(); });
    return true;
}
}  // namespace rtype::net::client
