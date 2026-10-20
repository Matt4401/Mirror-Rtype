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
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <span>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>

#include "Log/Logger.hpp"
#include "LogTag.hpp"
#include "Network/INetworkManager.hpp"
#include "Wire.hpp"
#include "session/TcpSession.hpp"

namespace rtype::net::server {

using wire::MessageKind;

NetworkServer::NetworkServer() : _work(asio::make_work_guard(_io)), _acceptor(_io), _udpSocket(_io), _udpTimer(_io) {}

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
        log::error(LOG_TAG, "cannot start server on port ", port, ": ", e.what());
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
            log::warning(LOG_TAG, "server full (", MAX_CLIENTS, " clients): connection refused");
            std::error_code ignored;
            std::ignore = socket.close(ignored);
        } else if (!ec) {
            const ClientId id = _nextId++;
            std::error_code endpointError;
            const auto remote = socket.remote_endpoint(endpointError);
            if (!endpointError) {
                log::debug(LOG_TAG, "TCP connection from ", remote, " accepted as client ", id);
            }
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
            log::debug(LOG_TAG, "client ", client, " opens UDP");
            info.udpOpen = true;
            info.udp.reset();
            info.lastSeen = std::chrono::steady_clock::now();
            break;
        case MessageKind::UdpClose:
            log::debug(LOG_TAG, "client ", client, " closes UDP");
            info.udpOpen = false;
            info.udp.reset();
            break;
        default:
            log::warning(LOG_TAG, "client ", client, " sent an unknown message kind (", static_cast<int>(kind),
                         "): disconnected");
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
                       ec != asio::error::message_size) {
                log::warning(LOG_TAG, "udp receive error: ", ec.message());
            }
            receiveUdp();
        });
}

void NetworkServer::handleDatagram(const std::size_t size) {
    if (size < wire::UDP_HEADER_SIZE || size > wire::MAX_DATAGRAM) {
        log::debug(LOG_TAG, "datagram of ", size, " bytes from ", _udpFrom, " dropped: invalid size");
        return;
    }
    const std::span<const std::uint8_t> datagram(_udpBuffer.data(), size);
    const ClientId id = wire::readU32(datagram.first<4>());
    const std::uint32_t token = wire::readU32(datagram.subspan<4, 4>());

    const auto it = _clients.find(id);
    if (it == _clients.end() || it->second.token != token || !it->second.udpOpen) {
        log::debug(LOG_TAG, "datagram from ", _udpFrom, " dropped: unknown client ", id, ", bad token or UDP not open");
        return;
    }
    ClientInfo& info = it->second;
    const bool firstBinding = !info.udp;
    info.udp = _udpFrom;
    info.lastSeen = std::chrono::steady_clock::now();
    if (firstBinding) {
        log::debug(LOG_TAG, "client ", id, " UDP bound to ", _udpFrom);
        sendDatagram(info, {});
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
    if (!client.udp.has_value()) {
        return;
    }
    const auto buffer = std::make_shared<Bytes>(std::move(datagram));
    client.lastSent = std::chrono::steady_clock::now();
    _udpSocket.async_send_to(asio::buffer(*buffer), client.udp.value(), [buffer](std::error_code, std::size_t) {});
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
            log::warning(LOG_TAG, "client ", info.id, " timed out on UDP: disconnected");
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
