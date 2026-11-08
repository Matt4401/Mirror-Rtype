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
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "ServerManager.hpp"
#include "gameplay/GameplayPlugin.hpp"
#include "level/Level.hpp"
#include "level/LevelLoader.hpp"
#include "net/NetworkFactory.hpp"
#include "parsing/Parsing.hpp"
#include "protocol/Messages.hpp"

namespace {
constexpr int EXIT_ERROR = 84;
/** @brief Level played by every room, relative to the directory the server is started from (the project root). */
constexpr const char* LEVEL_PATH = "server/levels/level_01.toml";

// The signal handler cannot take arguments, so it reaches the server through a global.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<rtype::server::ServerManager*> g_server{nullptr};

/**
 * @brief Loads the level played by every room.
 *
 * A level that cannot be read is not fatal: the server still runs, with no enemy, so that it can start without its data
 * files (from another directory, in a test environment...).
 *
 * @return The level, or `nullptr` if it cannot be loaded.
 */
std::shared_ptr<const rtype::server::level::Level> loadDefaultLevel() {
    std::vector<std::string> warnings;
    auto level =
        rtype::server::level::loadLevel(LEVEL_PATH, rtype::server::gameplay::GameplayPlugin::canSpawn, warnings);
    for (const std::string& warning : warnings) {
        rtype::log::warning(rtype::server::LOG_TAG, "level: ", warning);
    }
    if (!level) {
        rtype::log::warning(rtype::server::LOG_TAG, "cannot load the level: ", level.error(),
                            "; the rooms play without a level (run the server from the project root)");
        return nullptr;
    }
    rtype::log::info(rtype::server::LOG_TAG, "level '", level->name, "' loaded: ", level->timeline.events().size(),
                     " events");
    return std::make_shared<const rtype::server::level::Level>(std::move(*level));
}

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

    const std::shared_ptr<const rtype::server::level::Level> level = loadDefaultLevel();
    rtype::server::ServerManager server(ecs::net::createServer({.protocolId = rtype::protocol::TRANSPORT_PROTOCOL_ID}),
                                        rtype::server::HANDSHAKE_TIMEOUT, level);

    g_server = &server;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    const bool ok = server.run(options->port);
    g_server = nullptr;
    return ok ? 0 : EXIT_ERROR;
}
