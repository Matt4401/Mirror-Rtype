
#include "NetworkServer.hpp"

#include <iostream>
#include <ranges>

namespace rtype::net::server {

NetworkServer::NetworkServer()
    : _work(asio::make_work_guard(_io)), _acceptor(_io), _udpSocket(_io), _udpBuffer{}, _timeoutTimer(_io) {}

void NetworkServer::accept() {
    _acceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        if (ec == asio::error::operation_aborted) {
            return;
        }
        if (!ec) {
            const ClientId id = _nextId++;
            auto tcp = std::make_shared<session::TcpSession>(std::move(socket), id, _events,
                                                             [this](const ClientId closed) { onTcpClosed(closed); });
            _clients[id] = ClientInfo{.id = id,
                                      .token = std::uniform_int_distribution<std::uint32_t>{}(_random),
                                      .tcp = tcp,
                                      .udp = std::nullopt,
                                      .lastSeen = std::chrono::steady_clock::now()};
            _events.push({.type = EventType::Connected, .client = id, .data = {}, .channel = Channel::Reliable});
            tcp->start();
        }
        accept();
    });
}

void NetworkServer::onTcpClosed(const ClientId client) {
    const auto it = _clients.find(client);
    if (it == _clients.end()) {
        return;
    }
    if (it->second.udp) {
        _udpToClient.erase(*it->second.udp);
    }
    _clients.erase(it);
    _events.push({.type = EventType::Disconnected, .client = client, .data = {}, .channel = Channel::Reliable});
}

void NetworkServer::receiveUdp() {
    _udpSocket.async_receive_from(
        asio::buffer(_udpBuffer), _udpFrom, [this](const std::error_code ec, std::size_t size) {
            if (ec == asio::error::operation_aborted) {
                return;
            }
            if (!ec) {
                // TODO: handleUdpPacket(_udpFrom, _udpBuffer.data(), size);
            } else if (ec != asio::error::connection_reset && ec != asio::error::connection_refused) {
                std::cerr << "[network] udp receive error: " << ec.message() << '\n';
            }
            receiveUdp();
        });
}

void NetworkServer::scheduleTimeoutCheck() {
    _timeoutTimer.expires_after(std::chrono::seconds(1));
    _timeoutTimer.async_wait([this](const std::error_code ec) {
        if (ec == asio::error::operation_aborted) {
            return;
        }
        // TODO: checkTimeouts();
        scheduleTimeoutCheck();
    });
}

bool NetworkServer::start(const std::uint16_t port) {
    try {
        _acceptor = asio::ip::tcp::acceptor(_io, {asio::ip::tcp::v4(), port});
        _udpSocket = asio::ip::udp::socket(_io, {asio::ip::udp::v4(), port});
    } catch ([[maybe_unused]] const asio::system_error& e) {
        std::cerr << "[network] cannot start server on port " << port << ": " << e.what() << std::endl;
        std::error_code ignored = asio::error::operation_aborted;
        _acceptor.close(ignored);
        _udpSocket.close(ignored);
        return false;
    }

    accept();
    receiveUdp();
    scheduleTimeoutCheck();

    _running = true;
    _thread = std::thread([this] { _io.run(); });
    return true;
}

void NetworkServer::stop() {
    if (!_running.exchange(false)) {
        return;
    }

    asio::post(_io, [this] {
        std::error_code ignored = asio::error::operation_aborted;
        _acceptor.close(ignored);
        _udpSocket.close(ignored);
        _timeoutTimer.cancel();

        auto clients = std::move(_clients);
        _clients.clear();
        for (const auto& info : clients | std::views::values) {
            info.tcp->close();
        }
        _udpToClient.clear();
    });

    _work.reset();
    if (_thread.joinable()) {
        _thread.join();
    }
}

NetworkServer::~NetworkServer() { NetworkServer::stop(); }

void NetworkServer::kick(const ClientId client) {
    asio::post(_io, [this, client] {
        if (const auto it = _clients.find(client); it != _clients.end()) {
            it->second.tcp->close();
        }
    });
}

void NetworkServer::send(const ClientId client, const Bytes& data, const Channel channel) {
    asio::post(_io, [this, client, data, channel] { sendNow(client, data, channel); });
}

void NetworkServer::broadcast(const Bytes& data, const Channel channel) {
    asio::post(_io, [this, data, channel] {
        for (const auto& clientId : _clients | std::views::keys) {
            sendNow(clientId, data, channel);
        }
    });
}

void NetworkServer::sendNow(const ClientId client, const Bytes& data, const Channel channel) {
    const auto it = _clients.find(client);
    if (it == _clients.end()) {
        return;
    }

    if (channel == Channel::Reliable) {
        it->second.tcp->send(data);
        return;
    }
    if (!it->second.udp) {
        return;
    }
    const auto datagram = std::make_shared<Bytes>(data);
    _udpSocket.async_send_to(asio::buffer(*datagram), *it->second.udp, [](std::error_code, std::size_t) {});
}

}  // namespace rtype::net::server
