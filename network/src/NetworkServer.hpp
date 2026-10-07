#pragma once
#include <array>
#include <asio.hpp>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "EventQueue.hpp"
#include "Network/INetworkManager.hpp"
#include "Network/INetworkServer.hpp"
#include "session/TcpSession.hpp"

namespace rtype::net::server {

struct ClientInfo {
    ClientId id;
    uint32_t token;
    std::shared_ptr<session::TcpSession> tcp;
    std::optional<asio::ip::udp::endpoint> udp;
    std::chrono::steady_clock::time_point lastSeen;
};

class NetworkServer : public INetworkServer {
  public:
    explicit NetworkServer();
    ~NetworkServer() override;

    void accept();
    void receiveUdp();
    void scheduleTimeoutCheck();

    std::vector<NetworkEvent> poll() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;

    bool start(std::uint16_t port) override;
    void send(ClientId client, const Bytes& data, Channel channel) override;
    void broadcast(const Bytes& data, Channel channel) override;
    void kick(ClientId client) override;

  private:
    asio::io_context _io;
    asio::executor_work_guard<asio::io_context::executor_type> _work;
    std::thread _thread;

    asio::ip::tcp::acceptor _acceptor;

    asio::ip::udp::socket _udpSocket;
    asio::ip::udp::endpoint _udpFrom;
    std::array<uint8_t, 1500> _udpBuffer;

    asio::steady_timer _timeoutTimer;

    std::unordered_map<ClientId, ClientInfo> _clients;
    std::map<asio::ip::udp::endpoint, ClientId> _udpToClient;
    ClientId _nextId = 0;

    queue::EventQueue _events;
    std::atomic<bool> _running = false;
};

}  // namespace rtype::net::server
