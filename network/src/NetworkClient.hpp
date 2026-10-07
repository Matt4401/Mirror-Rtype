#pragma once

#include <asio.hpp>
#include <cstdint>
#include <string>
#include <vector>

#include "EventQueue.hpp"
#include "Network/INetworkClient.hpp"
#include "Network/INetworkManager.hpp"

namespace rtype::net::client {

static constexpr std::uint32_t MAX_TCP_MESSAGE = 64 * 1024;

class NetworkClient : public INetworkClient {
  public:
    explicit NetworkClient();
    ~NetworkClient() override;
    void readTcpHeader();
    void readTcpBody(std::uint32_t size);
    void onTcpError(std::error_code ec);
    void receiveUdp();
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    NetworkClient(NetworkClient&&) = delete;
    NetworkClient& operator=(NetworkClient&&) = delete;

    std::vector<NetworkEvent> poll() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;

    bool connect(const std::string& host, std::uint16_t port) override;
    bool openUdp(uint16_t port) override;
    void closeUdp() override;
    void send(const Bytes& data, Channel channel) override;

  private:
    asio::io_context _io;
    asio::executor_work_guard<asio::io_context::executor_type> _work;
    std::thread _thread;
    std::atomic<bool> _running{false};

    asio::ip::tcp::socket _tcpSocket;
    asio::ip::udp::socket _udpSocket;
    asio::ip::udp::endpoint _serverUdp;
    asio::ip::udp::endpoint _udpFrom;
    std::array<std::uint8_t, 1500> _udpBuffer{};

    std::array<std::uint8_t, 4> _tcpHeader;
    std::vector<std::uint8_t> _tcpBody;

    queue::EventQueue _events;
};

}  // namespace rtype::net::client
