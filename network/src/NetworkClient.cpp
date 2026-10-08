#include "NetworkClient.hpp"

#include <future>
#include <iostream>

namespace rtype::net::client {
NetworkClient::NetworkClient() : _work(asio::make_work_guard(_io)), _udpSocket(_io) {}

NetworkClient::~NetworkClient() { NetworkClient::stop(); }

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
    if (_running.load()) {
        return false;
    }

    asio::ip::tcp::socket socket(_io);
    try {
        asio::ip::tcp::resolver resolver(_io);
        asio::connect(socket, resolver.resolve(host, std::to_string(port)));
        _serverAddress = socket.remote_endpoint().address();
    } catch (asio::system_error& e) {
        std::cerr << "[network] cannot connect to " << host << ':' << port << ": " << e.what() << '\n';
        return false;
    }

    // The client has a single connection: its ClientId is always 0.
    _tcp = std::make_shared<session::TcpSession>(std::move(socket), 0, _events, [this](const ClientId client) {
        _events.push({.type = EventType::Disconnected, .client = client, .data = {}, .channel = Channel::Reliable});
    });
    _tcp->start();

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
            _serverUdp = {_serverAddress, port};
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

void NetworkClient::send(const Bytes& data, const Channel channel) {
    if (!_running.load()) {
        return;
    }

    asio::post(_io, [this, data, channel] {
        if (channel == Channel::Reliable) {
            _tcp->send(data);
            return;
        }
        if (!_udpSocket.is_open()) {
            return;
        }
        auto datagram = std::make_shared<Bytes>(data);
        _udpSocket.async_send_to(asio::buffer(*datagram), _serverUdp, [datagram](std::error_code, std::size_t) {});
    });
}

void NetworkClient::stop() {
    if (!_running.exchange(false)) {
        return;
    }

    asio::post(_io, [this] {
        std::error_code ignored;
        _udpSocket.close(ignored);
        _tcp->close();
    });

    _work.reset();
    if (_thread.joinable()) {
        _thread.join();
    }
}

}  // namespace rtype::net::client
