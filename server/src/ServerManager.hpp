#pragma once

#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Network/INetworkServer.hpp"
#include "Room.hpp"

namespace rtype::server {

class ServerManager {
  public:
    ServerManager() = default;
    ~ServerManager() = default;

    ServerManager(const ServerManager&) = delete;
    ServerManager& operator=(const ServerManager&) = delete;
    ServerManager(ServerManager&&) = delete;
    ServerManager& operator=(ServerManager&&) = delete;

    void run(std::uint16_t port);
    void stop();

  private:
    bool tick();
    void handleEvents();
    void update();
    std::unique_ptr<net::server::INetworkServer> _network;
    std::vector<std::unique_ptr<Room>> _rooms;
    std::atomic<bool> _running{false};
};

}  // namespace rtype::server
