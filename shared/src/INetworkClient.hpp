#pragma once

#include <cstdint>
#include <string>

#include "INetworkManager.hpp"

namespace rtype::net::client {
class INetworkClient : public INetworkManager {
  public:
    virtual bool connect(const std::string& host, std::uint16_t port) = 0;
    virtual bool openUdp(uint16_t port) = 0;
    virtual void closeUdp() = 0;
    virtual void send(const Bytes& data, Channel channel) = 0;
};

}  // namespace rtype::net::client
