/**
 * @file main.cpp
 * @brief RLSS server entry point: `rlss_server [port]`.
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <thread>

#include "CommandLine.hpp"
#include "Logger.hpp"
#include "MatchServer.hpp"
#include "net/INetworkServer.hpp"
#include "net/NetworkFactory.hpp"
#include "protocol/Protocol.hpp"

namespace {

constexpr int EXIT_ERROR = 84;
constexpr const char* LOG_TAG = "server";
/** @brief Pause between two polls of the transport, so that an idle server does not spin a core. */
constexpr std::chrono::milliseconds FRAME_SLEEP{1};

// The signal handler cannot take arguments, so it reaches the main loop through a global.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> g_running{true};

void onSignal(int /*signal*/) { g_running = false; }

}  // namespace

int main(int ac, char** av) {
    const std::span args(av, static_cast<std::size_t>(ac));
    const auto values = args.subspan(1);
    std::optional<std::uint16_t> port = rlss::protocol::DEFAULT_PORT;
    if (values.size() > 1) {
        port.reset();
    } else if (!values.empty()) {
        port = rlss::parsePort(values.front());
    }
    if (!port) {
        std::cerr << "usage: " << args.front() << " [port]\n";
        return EXIT_ERROR;
    }

    try {
        const std::shared_ptr<ecs::net::INetworkServer> transport =
            ecs::net::createServer({.protocolId = rlss::protocol::PROTOCOL_ID});
        if (!transport->start(*port)) {
            rtype::log::error(LOG_TAG, "cannot listen on port ", *port);
            return EXIT_ERROR;
        }
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);
        rtype::log::info(LOG_TAG, rlss::protocol::GAME_TITLE, " server listening on port ", *port);

        {
            // The menu runs here; each match runs on its own thread, with its own engine.
            rlss::server::MatchServer server(transport);
            while (g_running.load()) {
                server.update();
                std::this_thread::sleep_for(FRAME_SLEEP);
            }
            rtype::log::info(LOG_TAG, "shutting down");
        }
        transport->stop();
    } catch (const std::exception& error) {
        std::cerr << "server error: " << error.what() << '\n';
        return EXIT_ERROR;
    }
    return 0;
}
