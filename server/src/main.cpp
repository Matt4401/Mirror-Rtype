/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Server entry point
*/

#include <atomic>
#include <csignal>
#include <cstddef>
#include <iostream>
#include <span>

#include "Logger.hpp"
#include "Network/NetworkFactory.hpp"
#include "ServerManager.hpp"
#include "parsing/Parsing.hpp"

namespace {
constexpr int EXIT_ERROR = 84;

// The signal handler cannot take arguments, so it reaches the server through a global.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<rtype::server::ServerManager*> g_server{nullptr};

void onSignal(int /*signal*/) {
    if (auto* server = g_server.load()) {
        server->stop();  // only sets an atomic flag: signal-safe
    }
}
}  // namespace

int main(int ac, char** av) {
    const std::span args(av, static_cast<std::size_t>(ac));
    const auto options = rtype::server::parseOptions(args);
    if (!options) {
        std::cerr << rtype::server::usage(args.front()) << '\n';
        return EXIT_ERROR;
    }
    rtype::log::setVerbose(options->logs);
    rtype::log::setColored(options->colors);

    rtype::server::ServerManager server(rtype::net::createServer());

    g_server = &server;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    const bool ok = server.run(options->port);
    g_server = nullptr;
    return ok ? 0 : EXIT_ERROR;
}
