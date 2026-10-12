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

#include "EventQueue.hpp"
#include "Network/INetworkManager.hpp"
#include "Network/INetworkServer.hpp"
#include "Wire.hpp"
#include "session/TcpSession.hpp"

namespace rtype::net::server {

inline constexpr std::size_t MAX_CLIENTS = 64;

struct ClientInfo {
    ClientId id;
    std::uint32_t token;
    std::shared_ptr<session::TcpSession> tcp;
    bool udpOpen = false;
    std::optional<asio::ip::udp::endpoint> udp{};
    std::chrono::steady_clock::time_point lastSeen{};
    std::chrono::steady_clock::time_point lastSent{};
};

class NetworkServer : public INetworkServer {
  public:
    explicit NetworkServer();
    ~NetworkServer() override;

    std::vector<NetworkEvent> poll() override { return _events.popAll(); }
    void stop() override;
    [[nodiscard]] bool isRunning() const override { return _state.load() == State::Running; }

    bool start(std::uint16_t port) override;
    void send(ClientId client, const Bytes& data, Channel channel) override;
    void broadcast(const Bytes& data, Channel channel) override;
    void kick(ClientId client) override;

  private:
    void accept();
    void onTcpMessage(ClientId client, wire::MessageKind kind, Bytes payload);
    void onTcpClosed(ClientId client);

    void receiveUdp();
    void handleDatagram(std::size_t size);
    void sendNow(ClientId client, const Bytes& data, Channel channel);
    void sendDatagram(ClientInfo& client, Bytes datagram);

    void scheduleUdpTick();
    void onUdpTick();

    asio::io_context _io;
    asio::executor_work_guard<asio::io_context::executor_type> _work;
    std::thread _thread;

    asio::ip::tcp::acceptor _acceptor;

    asio::ip::udp::socket _udpSocket;
    asio::ip::udp::endpoint _udpFrom;
    std::array<uint8_t, 1500> _udpBuffer;

    asio::steady_timer _udpTimer;

    std::unordered_map<ClientId, ClientInfo> _clients;
    ClientId _nextId = 1;
    std::random_device _random;

    queue::EventQueue _events;
    std::atomic<State> _state{State::Idle};
};

}  // namespace rtype::net::server
