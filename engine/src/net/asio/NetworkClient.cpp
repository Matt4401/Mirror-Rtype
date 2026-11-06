#include "net/asio/NetworkClient.hpp"

#include <algorithm>
#include <asio/buffer.hpp>
#include <asio/connect.hpp>  // IWYU pragma: keep
#include <asio/error.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/post.hpp>
#include <asio/system_error.hpp>
#include <asio/use_future.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <span>
#include <string>
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

NetworkClient::NetworkClient(const NetworkConfig& config)
    : _config(config),
      _work(asio::make_work_guard(_io)),
      _resolver(_io),
      _connecting(_io),
      _udpSocket(_io),
      _udpTimer(_io) {}

NetworkClient::~NetworkClient() { NetworkClient::stop(); }

bool NetworkClient::connect(const std::string& host, const std::uint16_t port) {
    auto expected = State::Idle;
    if (!_state.compare_exchange_strong(expected, State::Running)) {
        return false;
    }

    // Resolving and connecting can take minutes when the host does not answer: both run on the network thread.
    _resolver.async_resolve(host, std::to_string(port),
                            [this](const std::error_code ec, const asio::ip::tcp::resolver::results_type& hosts) {
                                onResolved(ec, hosts);
                            });

    _thread = std::thread([this] { _io.run(); });
    return true;
}

void NetworkClient::onResolved(const std::error_code ec, const asio::ip::tcp::resolver::results_type& hosts) {
    // stop() may have run first: a resolution already finished is not cancelled, and connecting now would reopen the
    // socket it closed, leaving a session nobody closes.
    if (_state.load() != State::Running) {
        return;
    }
    if (ec) {
        onConnectFailed(ec);
        return;
    }
    // NOLINTNEXTLINE(misc-include-cleaner): false positive, the public header asio/connect.hpp is included
    asio::async_connect(_connecting, hosts, [this](const std::error_code error, const asio::ip::tcp::endpoint& server) {
        if (_state.load() != State::Running) {
            return;
        }
        if (error) {
            onConnectFailed(error);
        } else {
            onTcpConnected(server.address());
        }
    });
}

void NetworkClient::onConnectFailed(const std::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        return;
    }
    rtype::log::error(LOG_TAG, "cannot connect to the server: ", ec.message());
    _disconnectReason = DisconnectReason::Unreachable;
    _state = State::Stopped;
    _events.push({.type = EventType::Disconnected, .peer = 0, .data = {}, .channel = Channel::Reliable});
}

void NetworkClient::onTcpConnected(const asio::ip::address& server) {
    _serverAddress = server;
    _tcp = std::make_shared<TcpSession>(
        std::move(_connecting), 0,
        TcpSession::Limits{.maxPayload = _config.maxReliablePayload, .maxQueueBytes = _config.maxReliableQueueBytes},
        [this](PeerId, const MessageKind kind, Bytes payload) { onTcpMessage(kind, std::move(payload)); },
        [this](const PeerId peer) {
            _state = State::Stopped;
            closeUdpNow();
            _events.push({.type = EventType::Disconnected, .peer = peer, .data = {}, .channel = Channel::Reliable});
        });
    _tcp->start();

    // First frame: the server refuses the connection if it runs another protocol.
    Bytes handshake(wire::HANDSHAKE_SIZE);
    wire::writeU32(std::span(handshake).first<4>(), _config.protocolId);
    _tcp->send(MessageKind::Handshake, handshake);
}

void NetworkClient::onTcpMessage(const MessageKind kind, Bytes payload) {
    switch (kind) {
        case MessageKind::Data:
            _events.push(
                {.type = EventType::Data, .peer = 0, .data = std::move(payload), .channel = Channel::Reliable});
            break;
        case MessageKind::Welcome:
            if (_welcomed || payload.size() != wire::WELCOME_SIZE) {
                _tcp->close();
                return;
            }
            _peerId = wire::readU32(std::span(payload).first<4>());
            _token = wire::readU32(std::span(payload).subspan<4, 4>());
            _welcomed = true;
            _events.push({.type = EventType::Connected, .peer = 0, .data = {}, .channel = Channel::Reliable});
            break;
        case MessageKind::Refused:
            _disconnectReason = wire::readRefusal(payload);
            rtype::log::warning(LOG_TAG, "refused by the server");
            _tcp->close();
            break;
        default:
            _tcp->close();
            break;
    }
}

bool NetworkClient::openUdp(const std::uint16_t port) {
    if (_state.load() != State::Running) {
        return false;
    }
    auto open = [this, port] {
        if (!_welcomed) {
            rtype::log::error(LOG_TAG, "cannot open UDP: no Welcome received from the server yet");
            return false;
        }
        try {
            _serverUdp = {_serverAddress, port};
            _udpSocket = asio::ip::udp::socket(_io, {_serverUdp.protocol(), 0});
        } catch (const asio::system_error& e) {
            rtype::log::error(LOG_TAG, "cannot open UDP to port ", port, ": ", e.what());
            return false;
        }
        _tcp->send(MessageKind::UdpOpen);
        _lastUdpReceived = std::chrono::steady_clock::now();
        _udpBound = false;
        sendDatagram({});
        receiveUdp();
        scheduleUdpTick();
        return true;
    };
    std::future<bool> opened = asio::post(_io, asio::use_future(open));
    return opened.get();
}

void NetworkClient::closeUdp() {
    if (_state.load() != State::Running) {
        return;
    }

    asio::post(_io, [this] {
        if (!_udpSocket.is_open()) {
            return;
        }
        _tcp->send(MessageKind::UdpClose);
        closeUdpNow();
    });
}

void NetworkClient::closeUdpNow() {
    std::error_code ignored;
    std::ignore = _udpSocket.close(ignored);
    _udpTimer.cancel();
}

void NetworkClient::receiveUdp() {
    _udpSocket.async_receive_from(asio::buffer(_udpBuffer), _udpFrom,
                                  [this](const std::error_code ec, const std::size_t size) {
                                      // A datagram received just before stop() must not start a receive on the closed
                                      // socket: it would fail at once, in a loop that keeps stop() from joining.
                                      if (ec == asio::error::operation_aborted || _state.load() != State::Running) {
                                          return;
                                      }
                                      if (!ec && _udpFrom == _serverUdp && size <= MAX_DATAGRAM) {
                                          _lastUdpReceived = std::chrono::steady_clock::now();
                                          _udpBound = true;
                                          if (size > 0) {
                                              const auto payload = std::span(_udpBuffer).first(size);
                                              _events.push({.type = EventType::Data,
                                                            .peer = 0,
                                                            .data = Bytes(payload.begin(), payload.end()),
                                                            .channel = Channel::Unreliable});
                                          }
                                      }
                                      receiveUdp();
                                  });
}

void NetworkClient::sendDatagram(const Bytes& payload) {
    const auto datagram = std::make_shared<Bytes>(DATAGRAM_HEADER_SIZE + payload.size());
    wire::writeU32(std::span(*datagram).first<4>(), _peerId);
    wire::writeU32(std::span(*datagram).subspan<4, 4>(), _token);
    std::ranges::copy(payload, datagram->begin() + DATAGRAM_HEADER_SIZE);

    _lastUdpSent = std::chrono::steady_clock::now();
    _udpSocket.async_send_to(asio::buffer(*datagram), _serverUdp, [datagram](std::error_code, std::size_t) {});
}

void NetworkClient::scheduleUdpTick() {
    _udpTimer.expires_after(_config.udpTick);
    _udpTimer.async_wait([this](const std::error_code ec) {
        // cancel() has no effect on a timer that has just expired: its handler still runs, without error, after stop().
        if (ec || _state.load() != State::Running) {
            return;
        }
        onUdpTick();
    });
}

void NetworkClient::onUdpTick() {
    const auto now = std::chrono::steady_clock::now();

    if (now - _lastUdpReceived > _config.udpTimeout) {
        _tcp->close();
        return;
    }
    if (!_udpBound || now - _lastUdpSent >= _config.keepaliveInterval) {
        sendDatagram({});
    }
    scheduleUdpTick();
}

void NetworkClient::send(const Bytes& data, const Channel channel) {
    if (_state.load() != State::Running || !wire::isSendable(data, channel, _config.maxReliablePayload)) {
        return;
    }

    asio::post(_io, [this, data, channel] {
        if (channel == Channel::Reliable) {
            if (_tcp) {
                _tcp->send(MessageKind::Data, data);
            }
            return;
        }
        if (_udpSocket.is_open()) {
            sendDatagram(data);
        }
    });
}

void NetworkClient::stop() {
    const State previous = _state.exchange(State::Stopped);
    if (previous == State::Idle) {
        return;
    }
    if (previous == State::Running) {
        asio::post(_io, [this] {
            closeUdpNow();
            // Cancels a connection attempt still in progress.
            _resolver.cancel();
            std::error_code ignored;
            std::ignore = _connecting.close(ignored);
            if (_tcp) {
                _tcp->close();
            }
        });
    }
    _work.reset();
    if (_thread.joinable()) {
        _thread.join();
    }
}

}  // namespace ecs::net::detail
