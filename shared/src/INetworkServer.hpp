#pragma once

#include <cstdint>

#include "INetworkManager.hpp"

namespace rtype::net::server {

class INetworkServer : public INetworkManager {
  public:
    virtual bool start(std::uint16_t port) = 0;
    virtual void send(ClientId client, const Bytes& data, Channel channel) = 0;
    virtual void broadcast(const Bytes& data, Channel channel) = 0;
    virtual void kick(ClientId client) = 0;
};

}  // namespace rtype::net::server
