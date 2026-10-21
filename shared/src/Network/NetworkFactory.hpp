#pragma once

#include <memory>

#include "INetworkClient.hpp"
#include "INetworkServer.hpp"

// Creates the network implementations without exposing them. Game code only includes this header and the interfaces,
// so it never sees Asio. Defined in network/: link the "network" target.
namespace rtype::net {

[[nodiscard]] std::unique_ptr<server::INetworkServer> createServer();
[[nodiscard]] std::unique_ptr<client::INetworkClient> createClient();

}  // namespace rtype::net
