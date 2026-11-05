/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client entry point
*/

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include "RTypeGamePlugin.hpp"
#include "audio/AudioPlugin.hpp"
#include "core/Engine.hpp"
#include "game/ReplicationCodec.hpp"
#include "input/InputPlugin.hpp"
#include "net/ClientConnection.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkFactory.hpp"
#include "net/NetworkPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/ReplicationPlugins.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "protocol/Messages.hpp"
#include "render/RenderPlugin.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/render/AnimationPlugin.hpp"

namespace {

constexpr int EXIT_ERROR = 84;
constexpr std::string_view DEFAULT_HOST = "127.0.0.1";

struct Options {
    std::string host{DEFAULT_HOST};
    std::uint16_t port = rtype::protocol::DEFAULT_PORT;
};

/** @brief Reads a port number between 1 and 65535. */
bool parsePort(const std::string_view arg, std::uint16_t& port) {
    const char* const first = arg.data();
    const char* const last = std::next(first, static_cast<std::ptrdiff_t>(arg.size()));
    const auto [end, error] = std::from_chars(first, last, port);
    return error == std::errc{} && end == last && port != 0;
}

/** @brief Reads `[host] [port]`; returns nothing if the port is invalid or there are too many arguments. */
std::optional<Options> parseOptions(const std::span<char* const> args) {
    const auto values = args.subspan(1);
    constexpr std::size_t MAX_VALUES = 2;
    if (values.size() > MAX_VALUES) {
        return std::nullopt;
    }
    Options options;
    if (!values.empty()) {
        options.host = values.front();
    }
    if (values.size() == MAX_VALUES && !parsePort(values.back(), options.port)) {
        return std::nullopt;
    }
    return options;
}

}  // namespace

int main(int ac, char** av) {
    const std::span args(av, static_cast<std::size_t>(ac));
    const auto options = parseOptions(args);
    if (!options) {
        std::cerr << "usage: " << args.front() << " [host] [port]\n";
        return EXIT_ERROR;
    }
    try {
        const std::shared_ptr<ecs::net::INetworkClient> transport =
            ecs::net::createClient({.protocolId = rtype::protocol::TRANSPORT_PROTOCOL_ID});
        if (!transport->connect(options->host, options->port)) {
            std::cerr << "cannot reach the server at " << options->host << ':' << options->port << '\n';
            return EXIT_ERROR;
        }
        const auto connection = std::make_shared<engine::ClientConnection>(transport);

        engine::Engine engine;
        engine.addPlugin<render::RenderPlugin>()
            .addPlugin<input::InputPlugin>()
            .addPlugin<audio::AudioPlugin>()
            .addPlugin<systems::PhysicsPlugin>()
            .addPlugin<systems::AnimationPlugin>()
            .addPlugin<prefabs::PrefabPlugin>()
            .addPlugin<scripting::ScriptPlugin>()
            .addPlugin<ecs::net::NetworkPlugin>(connection, connection)
            .addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<rtype::game::ReplicationCodec>())
            .addPlugin<client::RTypeNetworkPlugin>(transport)
            .addPlugin<client::RTypeGamePlugin>();

        engine.run();
    } catch (const std::exception& error) {
        std::cerr << "client error: " << error.what() << '\n';
        return EXIT_ERROR;
    }
    return 0;
}
