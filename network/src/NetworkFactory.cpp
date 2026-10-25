#include "Network/NetworkFactory.hpp"

#include <memory>

#include "Network/INetworkClient.hpp"
#include "Network/INetworkServer.hpp"
#include "NetworkClient.hpp"
#include "NetworkServer.hpp"

namespace rtype::net {

std::unique_ptr<server::INetworkServer> createServer() { return std::make_unique<server::NetworkServer>(); }

std::unique_ptr<client::INetworkClient> createClient() { return std::make_unique<client::NetworkClient>(); }

}  // namespace rtype::net
