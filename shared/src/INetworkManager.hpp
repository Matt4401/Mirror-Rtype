
#pragma once

#include <cstdint>
#include <vector>

namespace net {

using ClientId = std::uint32_t;
using Bytes = std::vector<std::uint8_t>;

enum class EventType : std::uint8_t { Connected, Disconnected, Data };
enum class Channel : std::uint8_t { Reliable, Unreliable };

struct NetworkEvent {
    EventType type;
    ClientId client;
    Bytes data;
    Channel channel;
};

class INetworkManager {
  public:
    INetworkManager() = default;
    INetworkManager(const INetworkManager&) = delete;
    INetworkManager& operator=(const INetworkManager&) = delete;
    INetworkManager(INetworkManager&&) = delete;
    INetworkManager& operator=(INetworkManager&&) = delete;

    virtual ~INetworkManager() = default;
    virtual std::vector<NetworkEvent> poll() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;
};

}  // namespace net
