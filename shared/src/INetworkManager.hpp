
#pragma once

#include <cstdint>
#include <vector>

namespace net {

using ClientId = uint32_t;
using Bytes = std::vector<uint8_t>;

enum class EventType { Connected, Disconnected, Data };
enum class Channel { Reliable, Unreliable };

struct NetworkEvent {
    EventType type;
    ClientId client;
    Bytes data;
    Channel channel;
};

class INetworkManager {
  public:
    virtual ~INetworkManager() = default;
    virtual std::vector<NetworkEvent> poll() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
};

}  // namespace net
