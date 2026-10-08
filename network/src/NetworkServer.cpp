
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
            // TODO: créer la TcpSession, l'ajouter à _clients, pousser un event Connected
        }
        accept();
    });
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
        _clients.clear();
        _udpToClient.clear();
    });

    _work.reset();
    if (_thread.joinable()) {
        _thread.join();
    }
}

NetworkServer::~NetworkServer() { NetworkServer::stop(); }

void NetworkServer::kick(const ClientId client) {
    if (_clients.contains(client)) {
        _clients.erase(client);
    }
}

void NetworkServer::broadcast(const Bytes& data, const Channel channel) {
    for (const auto& clientId : _clients | std::views::keys) {
        send(clientId, data, channel);
    }
}

}  // namespace rtype::net::server
