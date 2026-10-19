/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Server entry point
*/

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

constexpr std::uint16_t DEFAULT_PORT = 4242;
constexpr int EXIT_ERROR = 84;

bool parsePort(const std::string_view arg, std::uint16_t& port) {
    const char* const first = std::to_address(arg.begin());
    const char* const last = std::to_address(arg.end());
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

    const bool ok = server.run(port);
    return ok ? 0 : EXIT_ERROR;
}
