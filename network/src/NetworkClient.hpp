#pragma once

#include <array>
#include <asio.hpp>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "EventQueue.hpp"
#include "Network/INetworkClient.hpp"
#include "Network/INetworkManager.hpp"
#include "session/TcpSession.hpp"

namespace rtype::net::client {

class NetworkClient : public INetworkClient {
  public:
    explicit NetworkClient();
    ~NetworkClient() override;
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    NetworkClient(NetworkClient&&) = delete;
    NetworkClient& operator=(NetworkClient&&) = delete;

    std::vector<NetworkEvent> poll() override { return _events.popAll(); }
    void stop() override;
    [[nodiscard]] bool isRunning() const override { return _running.load(); }

    bool connect(const std::string& host, std::uint16_t port) override;
    bool openUdp(uint16_t port) override;
    void closeUdp() override;
    void send(const Bytes& data, Channel channel) override;

    void receiveUdp();

  private:
    asio::io_context _io;
    asio::executor_work_guard<asio::io_context::executor_type> _work;
    std::thread _thread;
    std::atomic<bool> _running{false};

    std::shared_ptr<session::TcpSession> _tcp;
    asio::ip::address _serverAddress;

    asio::ip::udp::socket _udpSocket;
    asio::ip::udp::endpoint _serverUdp;
    asio::ip::udp::endpoint _udpFrom;
    std::array<std::uint8_t, 1500> _udpBuffer{};

    queue::EventQueue _events;
};

}  // namespace rtype::net::client
