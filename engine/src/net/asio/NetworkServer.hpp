#pragma once

#include <array>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkConfig.hpp"
#include "net/asio/EventQueue.hpp"
#include "net/asio/TcpSession.hpp"
#include "net/asio/Wire.hpp"

namespace ecs::net::detail {

/** @brief Transport state of one connection, owned by the network thread. */
struct PeerInfo {
    /** @brief Id given at accept time. */
    PeerId id = 0;
    /** @brief Random secret sent in `Welcome`, required in every client datagram. */
    std::uint32_t token = 0;
    /** @brief TCP connection. */
    std::shared_ptr<TcpSession> tcp;
    /** @brief True once a valid `Handshake` was received: only then does the game know this peer. */
    bool handshaken = false;
    /** @brief When the TCP connection was accepted, for the handshake timeout. */
    std::chrono::steady_clock::time_point acceptedAt;
    /** @brief True between `UdpOpen` and `UdpClose`. */
    bool udpOpen = false;
    /** @brief Source endpoint of the last valid datagram. */
    std::optional<asio::ip::udp::endpoint> udp;
    /** @brief Last valid datagram received, for the UDP timeout. */
    std::chrono::steady_clock::time_point lastSeen;
    /** @brief Last datagram sent, for keepalives. */
    std::chrono::steady_clock::time_point lastSent;
};

/** @brief Asio implementation of `INetworkServer`: one network thread, TCP + UDP on the same port. */
class NetworkServer : public INetworkServer {
  public:
    /** @brief Creates a stopped server. @param config Valid transport settings. */
    explicit NetworkServer(const NetworkConfig& config);
    /** @brief Stops the server and joins its thread. */
    ~NetworkServer() override;
    NetworkServer(const NetworkServer&) = delete;
    NetworkServer& operator=(const NetworkServer&) = delete;
    NetworkServer(NetworkServer&&) = delete;
    NetworkServer& operator=(NetworkServer&&) = delete;

    std::vector<NetworkEvent> poll() override { return _events.popAll(); }
    void stop() override;
    [[nodiscard]] bool isRunning() const override { return _state.load() == State::Running; }

    bool start(std::uint16_t port) override;
    void send(PeerId peer, const Bytes& data, Channel channel) override;
    void broadcast(const Bytes& data, Channel channel) override;
    void kick(PeerId peer) override;

  private:
    void accept();
    void onTcpMessage(PeerId peer, wire::MessageKind kind, Bytes payload);
    void onHandshake(PeerInfo& info, const Bytes& payload);
    void onTcpClosed(PeerId peer);

    void receiveUdp();
    void handleDatagram(std::size_t size);
    void sendNow(PeerId peer, const Bytes& data, Channel channel);
    void sendDatagram(PeerInfo& peer, Bytes datagram);

    void scheduleUdpTick();
    void onUdpTick();

    NetworkConfig _config;

    asio::io_context _io;
    asio::executor_work_guard<asio::io_context::executor_type> _work;
    std::thread _thread;

    asio::ip::tcp::acceptor _acceptor;

    asio::ip::udp::socket _udpSocket;
    asio::ip::udp::endpoint _udpFrom;
    std::array<std::uint8_t, 1500> _udpBuffer{};

    asio::steady_timer _udpTimer;

    std::unordered_map<PeerId, PeerInfo> _peers;
    PeerId _nextId = 1;
    std::random_device _random;

    EventQueue _events;
    std::atomic<State> _state{State::Idle};
};

}  // namespace ecs::net::detail
