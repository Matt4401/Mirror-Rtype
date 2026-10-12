#include "NetworkServer.hpp"

#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/post.hpp>
#include <asio/system_error.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <span>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>

#include "Network/INetworkManager.hpp"
#include "Wire.hpp"
#include "session/TcpSession.hpp"

namespace rtype::net::server {

using wire::MessageKind;

NetworkServer::NetworkServer()
    : _work(asio::make_work_guard(_io)), _acceptor(_io), _udpSocket(_io), _udpBuffer{}, _udpTimer(_io) {}

NetworkServer::~NetworkServer() { NetworkServer::stop(); }

bool NetworkServer::start(const std::uint16_t port) {
    auto expected = State::Idle;
    if (!_state.compare_exchange_strong(expected, State::Running)) {
        return false;
    }

    try {
        _acceptor = asio::ip::tcp::acceptor(_io, {asio::ip::tcp::v4(), port});
        _udpSocket = asio::ip::udp::socket(_io, {asio::ip::udp::v4(), port});
    } catch (const asio::system_error& e) {
        std::cerr << "[network] cannot start server on port " << port << ": " << e.what() << '\n';
        std::error_code ignored;
        std::ignore = _acceptor.close(ignored);
        std::ignore = _udpSocket.close(ignored);
        _state = State::Idle;
        return false;
    }

    accept();
    receiveUdp();
    scheduleUdpTick();

    _thread = std::thread([this] { _io.run(); });
    return true;
}

void NetworkServer::stop() {
    if (_state.exchange(State::Stopped) != State::Running) {
        return;
    }

    asio::post(_io, [this] {
        std::error_code ignored;
        std::ignore = _acceptor.close(ignored);
        std::ignore = _udpSocket.close(ignored);
        _udpTimer.cancel();

        // Moved out first so that onTcpClosed() finds nothing: no Disconnected events when the server itself stops.
        // The deferred closes are pending work: run() executes them before returning.
        auto clients = std::move(_clients);
        _clients.clear();
        for (const auto& info : clients | std::views::values) {
            info.tcp->close();
        }
    });

    _work.reset();
    if (_thread.joinable()) {
        _thread.join();
    }
}

void NetworkServer::accept() {
    _acceptor.async_accept([this](const std::error_code ec, asio::ip::tcp::socket socket) {
        if (ec == asio::error::operation_aborted) {
            return;
        }
        if (!ec && _clients.size() >= MAX_CLIENTS) {
            std::cerr << "[network] server full (" << MAX_CLIENTS << " clients): connection refused\n";
            std::error_code ignored;
            std::ignore = socket.close(ignored);
        } else if (!ec) {
            const ClientId id = _nextId++;
            const std::uint32_t token = std::uniform_int_distribution<std::uint32_t>{}(_random);
            const auto tcp = std::make_shared<session::TcpSession>(
                std::move(socket), id,
                [this](const ClientId from, const MessageKind kind, Bytes payload) {
                    onTcpMessage(from, kind, std::move(payload));
                },
                [this](const ClientId closed) { onTcpClosed(closed); });
            _clients[id] = ClientInfo{.id = id,
                                      .token = token,
                                      .tcp = tcp,
                                      .udpOpen = false,
                                      .udp = std::nullopt,
                                      .lastSeen = {},
                                      .lastSent = {}};
            _events.push({.type = EventType::Connected, .client = id, .data = {}, .channel = Channel::Reliable});

            // Internal message: the client library needs its ClientId and token to send UDP datagrams.
            Bytes welcome(wire::UDP_HEADER_SIZE);
            wire::writeU32(std::span(welcome).first<4>(), id);
            wire::writeU32(std::span(welcome).subspan<4, 4>(), token);
            tcp->send(MessageKind::Welcome, welcome);
            tcp->start();
        }
        accept();
    });
}

void NetworkServer::onTcpMessage(const ClientId client, const MessageKind kind, Bytes payload) {
    const auto it = _clients.find(client);
    if (it == _clients.end()) {
        return;
    }
    ClientInfo& info = it->second;

    switch (kind) {
        case MessageKind::Data:
            _events.push(
                {.type = EventType::Data, .client = client, .data = std::move(payload), .channel = Channel::Reliable});
            break;
        case MessageKind::UdpOpen:
            info.udpOpen = true;
            info.udp.reset();
            info.lastSeen = std::chrono::steady_clock::now();  // the 5 s timeout starts here
            break;
        case MessageKind::UdpClose:
            info.udpOpen = false;
            info.udp.reset();
            break;
        default:
            // Welcome only goes server -> client, and any other value is not a known kind: protocol violation.
            info.tcp->close();
            break;
    }
}

void NetworkServer::onTcpClosed(const ClientId client) {
    if (_clients.erase(client) == 0) {
        return;
    }
    _events.push({.type = EventType::Disconnected, .client = client, .data = {}, .channel = Channel::Reliable});
}

void NetworkServer::kick(const ClientId client) {
    asio::post(_io, [this, client] {
        if (const auto it = _clients.find(client); it != _clients.end()) {
            it->second.tcp->close();
        }
    });
}

void NetworkServer::receiveUdp() {
    _udpSocket.async_receive_from(
        asio::buffer(_udpBuffer), _udpFrom, [this](const std::error_code ec, const std::size_t size) {
            if (ec == asio::error::operation_aborted) {
                return;
            }
            if (!ec) {
                handleDatagram(size);
            } else if (ec != asio::error::connection_reset && ec != asio::error::connection_refused &&
                       ec != asio::error::message_size) {  // Windows: datagram bigger than the buffer
                std::cerr << "[network] udp receive error: " << ec.message() << '\n';
            }
            receiveUdp();
        });
}

void NetworkServer::handleDatagram(const std::size_t size) {
    if (size < wire::UDP_HEADER_SIZE || size > wire::MAX_DATAGRAM) {
        return;
    }
    const std::span<const std::uint8_t> datagram(_udpBuffer.data(), size);
    const ClientId id = wire::readU32(datagram.first<4>());
    const std::uint32_t token = wire::readU32(datagram.subspan<4, 4>());

    const auto it = _clients.find(id);
    // Unknown client, wrong token (spoofed), or UDP closed (late datagram sent before UdpClose): drop it.
    if (it == _clients.end() || it->second.token != token || !it->second.udpOpen) {
        return;
    }
    ClientInfo& info = it->second;
    const bool firstBinding = !info.udp;
    info.udp = _udpFrom;  // first datagram, or the NAT changed the client's port
    info.lastSeen = std::chrono::steady_clock::now();
    if (firstBinding) {
        sendDatagram(info, {});  // acts as an ack: the client stops resending its bind datagram
    }

    if (size > wire::UDP_HEADER_SIZE) {
        const auto payload = datagram.subspan(wire::UDP_HEADER_SIZE);
        _events.push({.type = EventType::Data,
                      .client = id,
                      .data = Bytes(payload.begin(), payload.end()),
                      .channel = Channel::Unreliable});
    }
}

void NetworkServer::sendDatagram(ClientInfo& client, Bytes datagram) {
    const auto buffer = std::make_shared<Bytes>(std::move(datagram));
    client.lastSent = std::chrono::steady_clock::now();
    // The capture keeps the buffer alive until the send completes: do not remove it.
    if (client.udp.has_value())
        _udpSocket.async_send_to(asio::buffer(*buffer), client.udp.value(), [](std::error_code, std::size_t) {});
}

void NetworkServer::scheduleUdpTick() {
    _udpTimer.expires_after(wire::UDP_TICK);
    _udpTimer.async_wait([this](const std::error_code ec) {
        if (ec) {
            return;
        }
        onUdpTick();
        scheduleUdpTick();
    });
}

void NetworkServer::onUdpTick() {
    const auto now = std::chrono::steady_clock::now();

    for (auto& info : _clients | std::views::values) {
        if (!info.udpOpen) {
            continue;
        }
        if (now - info.lastSeen > wire::UDP_TIMEOUT) {
            info.tcp->close();
        } else if (info.udp && now - info.lastSent >= wire::KEEPALIVE_INTERVAL) {
            sendDatagram(info, {});
        }
    }
}

void NetworkServer::send(const ClientId client, const Bytes& data, const Channel channel) {
    if (!wire::isSendable(data, channel)) {
        return;
    }
    asio::post(_io, [this, client, data, channel] { sendNow(client, data, channel); });
}

void NetworkServer::broadcast(const Bytes& data, const Channel channel) {
    if (!wire::isSendable(data, channel)) {
        return;
    }
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
    ClientInfo& info = it->second;

    if (channel == Channel::Reliable) {
        info.tcp->send(MessageKind::Data, data);
        return;
    }
    if (!info.udpOpen || !info.udp) {
        return;
    }
    sendDatagram(info, data);
}

}  // namespace rtype::net::server
