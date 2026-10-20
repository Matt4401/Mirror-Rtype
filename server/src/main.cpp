/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Server entry point
*/

#include <cstddef>
#include <iostream>
#include <span>

#include "Logger.hpp"
#include "Network/NetworkFactory.hpp"
#include "ServerManager.hpp"
#include "parsing/Parsing.hpp"

namespace {
constexpr int EXIT_ERROR = 84;
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

    const bool ok = server.run(options->port);
    return ok ? 0 : EXIT_ERROR;
}
