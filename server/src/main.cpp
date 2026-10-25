/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Server entry point
*/

#include <atomic>
#include <charconv>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>

#include "Network/NetworkFactory.hpp"
#include "ServerManager.hpp"

namespace {
constexpr int EXIT_ERROR = 84;
constexpr std::uint16_t DEFAULT_PORT = 4242;

// The signal handler cannot take arguments, so it reaches the server through a global.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<rtype::server::ServerManager*> g_server{nullptr};

void onSignal(int /*signal*/) {
    if (auto* server = g_server.load()) {
        server->stop();  // only sets an atomic flag: signal-safe
    }
}

bool parsePort(const std::string_view arg, std::uint16_t& port) {
    const char* first = std::to_address(arg.begin());
    const char* last = std::to_address(arg.end());
    const auto [end, ec] = std::from_chars(first, last, port);
    return ec == std::errc{} && end == last && port != 0;
}

}  // namespace

int main(int ac, char** av) {
    const std::span args(av, static_cast<std::size_t>(ac));
    std::uint16_t port = DEFAULT_PORT;
    if (args.size() > 2 || (args.size() == 2 && !parsePort(args.back(), port))) {
        std::cerr << "USAGE: " << args.front() << " [port]\n";
        return EXIT_ERROR;
    }

    rtype::server::ServerManager server(rtype::net::createServer());

    g_server = &server;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    const bool ok = server.run(port);
    g_server = nullptr;
    return ok ? 0 : EXIT_ERROR;
}
