#include "net/asio/NetworkServer.hpp"

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

#include "Logger.hpp"
#include "net/INetworkManager.hpp"
#include "net/NetworkConfig.hpp"
#include "net/asio/LogTag.hpp"
#include "net/asio/TcpSession.hpp"
#include "net/asio/Wire.hpp"

namespace ecs::net::detail {

using wire::MessageKind;

namespace {

/** @brief Sends the last frame of a session, which tells the client why it is closed, then closes the session. */
void closeWithReason(TcpSession& session, const DisconnectReason reason) {
    session.send(MessageKind::Refused, Bytes{static_cast<std::uint8_t>(reason)});
    session.closeAfterFlush();
}

}  // namespace

NetworkServer::NetworkServer(const NetworkConfig& config)
    : _config(config), _work(asio::make_work_guard(_io)), _acceptor(_io), _udpSocket(_io), _udpTimer(_io) {}

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
        rtype::log::error(LOG_TAG, "cannot start server on port ", port, ": ", e.what());
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

        auto peers = std::move(_peers);
        _peers.clear();
        for (const auto& info : peers | std::views::values) {
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
        if (!ec && _peers.size() >= _config.maxPeers) {
            rtype::log::warning(LOG_TAG, "server full (", _config.maxPeers, " peers): connection refused");
            refuseConnection(std::move(socket));
        } else if (!ec) {
            const PeerId id = _nextId++;
            std::error_code endpointError;
            const auto remote = socket.remote_endpoint(endpointError);
            if (!endpointError) {
                rtype::log::debug(LOG_TAG, "TCP connection from ", remote, " accepted as peer ", id);
            }
            const auto tcp = std::make_shared<TcpSession>(
                std::move(socket), id,
                TcpSession::Limits{.maxPayload = _config.maxReliablePayload,
                                   .maxQueueBytes = _config.maxReliableQueueBytes},
                [this](const PeerId from, const MessageKind kind, Bytes payload) {
                    onTcpMessage(from, kind, std::move(payload));
                },
                [this](const PeerId closed) { onTcpClosed(closed); });
            // The game does not know this peer yet: `Connected` is pushed once its `Handshake` is accepted.
            _peers[id] = PeerInfo{.id = id,
                                  .token = std::uniform_int_distribution<std::uint32_t>{}(_random),
                                  .tcp = tcp,
                                  .handshaken = false,
                                  .acceptedAt = std::chrono::steady_clock::now(),
                                  .udpOpen = false,
                                  .udp = std::nullopt,
                                  .lastSeen = {},
                                  .lastSent = {}};
            tcp->start();
        }
        accept();
    });
}

void NetworkServer::refuseConnection(asio::ip::tcp::socket socket) {
    // Not in `_peers`, and never read: its pending write keeps it alive until it is closed.
    const auto tcp = std::make_shared<TcpSession>(
        std::move(socket), 0,
        TcpSession::Limits{.maxPayload = _config.maxReliablePayload, .maxQueueBytes = _config.maxReliableQueueBytes},
        [](PeerId, MessageKind, const Bytes&) {}, [](PeerId) {});
    closeWithReason(*tcp, DisconnectReason::ServerFull);
}

void NetworkServer::onTcpMessage(const PeerId peer, const MessageKind kind, Bytes payload) {
    const auto it = _peers.find(peer);
    if (it == _peers.end()) {
        return;
    }
    PeerInfo& info = it->second;

    if (!info.handshaken) {
        if (kind == MessageKind::Handshake) {
            onHandshake(info, payload);
        } else {
            rtype::log::warning(LOG_TAG, "peer ", peer, " sent a frame before its handshake: disconnected");
            info.tcp->close();
        }
        return;
    }

    switch (kind) {
        case MessageKind::Data:
            _events.push(
                {.type = EventType::Data, .peer = peer, .data = std::move(payload), .channel = Channel::Reliable});
            break;
        case MessageKind::UdpOpen:
            rtype::log::debug(LOG_TAG, "peer ", peer, " opens UDP");
            info.udpOpen = true;
            info.udp.reset();
            info.lastSeen = std::chrono::steady_clock::now();
            break;
        case MessageKind::UdpClose:
            rtype::log::debug(LOG_TAG, "peer ", peer, " closes UDP");
            info.udpOpen = false;
            info.udp.reset();
            break;
        default:
            rtype::log::warning(LOG_TAG, "peer ", peer, " sent an unexpected message kind (", static_cast<int>(kind),
                                "): disconnected");
            info.tcp->close();
            break;
    }
}

void NetworkServer::onHandshake(PeerInfo& info, const Bytes& payload) {
    if (payload.size() != wire::HANDSHAKE_SIZE) {
        rtype::log::warning(LOG_TAG, "peer ", info.id, " sent a malformed handshake: disconnected");
        info.tcp->close();
        return;
    }
    const std::uint32_t protocolId = wire::readU32(std::span(payload).first<4>());
    if (protocolId != _config.protocolId) {
        rtype::log::warning(LOG_TAG, "peer ", info.id, " speaks protocol ", protocolId, " instead of ",
                            _config.protocolId, ": disconnected");
        closeWithReason(*info.tcp, DisconnectReason::ProtocolMismatch);
        return;
    }

    info.handshaken = true;
    _events.push({.type = EventType::Connected, .peer = info.id, .data = {}, .channel = Channel::Reliable});

    Bytes welcome(wire::WELCOME_SIZE);
    wire::writeU32(std::span(welcome).first<4>(), info.id);
    wire::writeU32(std::span(welcome).subspan<4, 4>(), info.token);
    info.tcp->send(MessageKind::Welcome, welcome);
}

void NetworkServer::onTcpClosed(const PeerId peer) {
    const auto it = _peers.find(peer);
    if (it == _peers.end()) {
        return;
    }
    const bool wasConnected = it->second.handshaken;
    _peers.erase(it);
    // A peer refused during the handshake was never announced to the game: nothing to report.
    if (wasConnected) {
        _events.push({.type = EventType::Disconnected, .peer = peer, .data = {}, .channel = Channel::Reliable});
    }
}

void NetworkServer::kick(const PeerId peer) {
    asio::post(_io, [this, peer] {
        if (const auto it = _peers.find(peer); it != _peers.end()) {
            it->second.tcp->close();
        }
    });
}

void NetworkServer::refuse(const PeerId peer, const DisconnectReason reason) {
    asio::post(_io, [this, peer, reason] {
        if (const auto it = _peers.find(peer); it != _peers.end()) {
            closeWithReason(*it->second.tcp, reason);
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
                rtype::log::warning(LOG_TAG, "udp receive error: ", ec.message());
            }
            receiveUdp();
        });
}

void NetworkServer::handleDatagram(const std::size_t size) {
    if (size < DATAGRAM_HEADER_SIZE || size > MAX_DATAGRAM) {
        rtype::log::debug(LOG_TAG, "datagram of ", size, " bytes from ", _udpFrom, " dropped: invalid size");
        return;
    }
    const std::span<const std::uint8_t> datagram(_udpBuffer.data(), size);
    const PeerId id = wire::readU32(datagram.first<4>());
    const std::uint32_t token = wire::readU32(datagram.subspan<4, 4>());

    // `udpOpen` is only set after the handshake, so a handshaking peer cannot use UDP.
    const auto it = _peers.find(id);
    if (it == _peers.end() || it->second.token != token || !it->second.udpOpen) {
        rtype::log::debug(LOG_TAG, "datagram from ", _udpFrom, " dropped: unknown peer ", id,
                          ", bad token or UDP not open");
        return;
    }
    PeerInfo& info = it->second;
    const bool firstBinding = !info.udp;
    info.udp = _udpFrom;
    info.lastSeen = std::chrono::steady_clock::now();
    if (firstBinding) {
        rtype::log::debug(LOG_TAG, "peer ", id, " UDP bound to ", _udpFrom);
        sendDatagram(info, {});
    }

    if (size > DATAGRAM_HEADER_SIZE) {
        const auto payload = datagram.subspan(DATAGRAM_HEADER_SIZE);
        _events.push({.type = EventType::Data,
                      .peer = id,
                      .data = Bytes(payload.begin(), payload.end()),
                      .channel = Channel::Unreliable});
    }
}

void NetworkServer::sendDatagram(PeerInfo& peer, Bytes datagram) {
    if (!peer.udp.has_value()) {
        return;
    }
    const auto buffer = std::make_shared<Bytes>(std::move(datagram));
    peer.lastSent = std::chrono::steady_clock::now();
    _udpSocket.async_send_to(asio::buffer(*buffer), peer.udp.value(), [buffer](std::error_code, std::size_t) {});
}

void NetworkServer::scheduleUdpTick() {
    _udpTimer.expires_after(_config.udpTick);
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

    // `close()` only posts the closing: `_peers` is not modified during this loop.
    for (auto& info : _peers | std::views::values) {
        if (!info.handshaken) {
            if (now - info.acceptedAt > _config.handshakeTimeout) {
                rtype::log::warning(LOG_TAG, "peer ", info.id, " did not complete its handshake: disconnected");
                info.tcp->close();
            }
            continue;
        }
        if (!info.udpOpen) {
            continue;
        }
        if (now - info.lastSeen > _config.udpTimeout) {
            rtype::log::warning(LOG_TAG, "peer ", info.id, " timed out on UDP: disconnected");
            info.tcp->close();
        } else if (info.udp && now - info.lastSent >= _config.keepaliveInterval) {
            sendDatagram(info, {});
        }
    }
}

void NetworkServer::send(const PeerId peer, const Bytes& data, const Channel channel) {
    if (!wire::isSendable(data, channel, _config.maxReliablePayload)) {
        return;
    }
    asio::post(_io, [this, peer, data, channel] { sendNow(peer, data, channel); });
}

void NetworkServer::broadcast(const Bytes& data, const Channel channel) {
    if (!wire::isSendable(data, channel, _config.maxReliablePayload)) {
        return;
    }
    asio::post(_io, [this, data, channel] {
        for (const auto& peerId : _peers | std::views::keys) {
            sendNow(peerId, data, channel);
        }
    });
}

void NetworkServer::sendNow(const PeerId peer, const Bytes& data, const Channel channel) {
    const auto it = _peers.find(peer);
    if (it == _peers.end() || !it->second.handshaken) {
        return;
    }
    PeerInfo& info = it->second;

    if (channel == Channel::Reliable) {
        info.tcp->send(MessageKind::Data, data);
        return;
    }
    if (!info.udpOpen || !info.udp) {
        return;
    }
    sendDatagram(info, data);
}

}  // namespace ecs::net::detail
