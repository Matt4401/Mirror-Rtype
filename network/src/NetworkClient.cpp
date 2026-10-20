#include "NetworkClient.hpp"

#include <algorithm>
#include <asio/buffer.hpp>
#include <asio/connect.hpp>  // IWYU pragma: keep
#include <asio/error.hpp>
#include <asio/executor_work_guard.hpp>
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

#include "Log/Logger.hpp"
#include "LogTag.hpp"
#include "Network/INetworkManager.hpp"
#include "Wire.hpp"
#include "session/TcpSession.hpp"

namespace rtype::net::client {

using wire::MessageKind;

NetworkClient::NetworkClient() : _work(asio::make_work_guard(_io)), _udpSocket(_io), _udpTimer(_io) {}

NetworkClient::~NetworkClient() { NetworkClient::stop(); }

bool NetworkClient::connect(const std::string& host, const std::uint16_t port) {
    auto expected = State::Idle;
    if (!_state.compare_exchange_strong(expected, State::Running)) {
        return false;
    }

    asio::ip::tcp::socket socket(_io);
    try {
        asio::ip::tcp::resolver resolver(_io);
        // NOLINTNEXTLINE(misc-include-cleaner): false positive, the public header asio/connect.hpp is included
        asio::connect(socket, resolver.resolve(host, std::to_string(port)));
        _serverAddress = socket.remote_endpoint().address();
    } catch (asio::system_error& e) {
        log::error(LOG_TAG, "cannot connect to ", host, ':', port, ": ", e.what());
        _state = State::Idle;
        return false;
    }

    _tcp = std::make_shared<session::TcpSession>(
        std::move(socket), 0,
        [this](ClientId, const MessageKind kind, Bytes payload) { onTcpMessage(kind, std::move(payload)); },
        [this](const ClientId client) {
            _state = State::Stopped;
            closeUdpNow();
            _events.push({.type = EventType::Disconnected, .client = client, .data = {}, .channel = Channel::Reliable});
        });
    _tcp->start();

    _thread = std::thread([this] { _io.run(); });
    return true;
}

void NetworkClient::onTcpMessage(const MessageKind kind, Bytes payload) {
    switch (kind) {
        case MessageKind::Data:
            _events.push(
                {.type = EventType::Data, .client = 0, .data = std::move(payload), .channel = Channel::Reliable});
            break;
        case MessageKind::Welcome:
            if (payload.size() != wire::UDP_HEADER_SIZE) {
                _tcp->close();
                return;
            }
            _clientId = wire::readU32(std::span(payload).first<4>());
            _token = wire::readU32(std::span(payload).subspan<4, 4>());
            _welcomed = true;
            _events.push({.type = EventType::Connected, .client = 0, .data = {}, .channel = Channel::Reliable});
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
            log::error(LOG_TAG, "cannot open UDP: no Welcome received from the server yet");
            return false;
        }
        try {
            _serverUdp = {_serverAddress, port};
            _udpSocket = asio::ip::udp::socket(_io, {_serverUdp.protocol(), 0});
        } catch (const asio::system_error& e) {
            log::error(LOG_TAG, "cannot open UDP to port ", port, ": ", e.what());
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
                                      if (ec == asio::error::operation_aborted) {
                                          return;
                                      }
                                      if (!ec && _udpFrom == _serverUdp && size <= wire::MAX_DATAGRAM) {
                                          _lastUdpReceived = std::chrono::steady_clock::now();
                                          _udpBound = true;
                                          if (size > 0) {
                                              const auto payload = std::span(_udpBuffer).first(size);
                                              _events.push({.type = EventType::Data,
                                                            .client = 0,
                                                            .data = Bytes(payload.begin(), payload.end()),
                                                            .channel = Channel::Unreliable});
                                          }
                                      }
                                      receiveUdp();
                                  });
}

void NetworkClient::sendDatagram(const Bytes& payload) {
    const auto datagram = std::make_shared<Bytes>(wire::UDP_HEADER_SIZE + payload.size());
    wire::writeU32(std::span(*datagram).first<4>(), _clientId);
    wire::writeU32(std::span(*datagram).subspan<4, 4>(), _token);
    std::ranges::copy(payload, datagram->begin() + wire::UDP_HEADER_SIZE);

    _lastUdpSent = std::chrono::steady_clock::now();
    _udpSocket.async_send_to(asio::buffer(*datagram), _serverUdp, [datagram](std::error_code, std::size_t) {});
}

void NetworkClient::scheduleUdpTick() {
    _udpTimer.expires_after(wire::UDP_TICK);
    _udpTimer.async_wait([this](const std::error_code ec) {
        if (ec) {
            return;
        }
        onUdpTick();
    });
}

void NetworkClient::onUdpTick() {
    const auto now = std::chrono::steady_clock::now();

    if (now - _lastUdpReceived > wire::UDP_TIMEOUT) {
        _tcp->close();
        return;
    }
    if (!_udpBound || now - _lastUdpSent >= wire::KEEPALIVE_INTERVAL) {
        sendDatagram({});
    }
    scheduleUdpTick();
}

void NetworkClient::send(const Bytes& data, const Channel channel) {
    if (_state.load() != State::Running || !wire::isSendable(data, channel)) {
        return;
    }

    asio::post(_io, [this, data, channel] {
        if (channel == Channel::Reliable) {
            _tcp->send(MessageKind::Data, data);
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
            _tcp->close();
        });
    }
    _work.reset();
    if (_thread.joinable()) {
        _thread.join();
    }
}

}  // namespace rtype::net::client
