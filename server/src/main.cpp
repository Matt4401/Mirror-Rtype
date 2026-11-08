/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Server entry point
*/

#include <atomic>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "LogTag.hpp"
#include "ServerManager.hpp"
#include "core/ContentRoot.hpp"
#include "ecs/time/PreciseSleep.hpp"
#include "gameplay/LevelDirectorPlugin.hpp"
#include "level/Level.hpp"
#include "level/LevelLoader.hpp"
#include "log/Logger.hpp"
#include "net/NetworkFactory.hpp"
#include "parsing/Parsing.hpp"
#include "protocol/Messages.hpp"

namespace {
constexpr int EXIT_ERROR = 84;
/** @brief Level played by every room, relative to the data directory (see `dataRoot()`). */
constexpr const char* LEVEL_PATH = "server/levels/level_01.toml";

// The signal handler cannot take arguments, so it reaches the server through a global.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<rtype::server::ServerManager*> g_server{nullptr};

/**
 * @brief Finds the directory holding the server data (`server/levels/`, `scripts/`): `engine::contentRoot()`, the
 * directory of the executable (which the build puts at the project root) unless `ENGINE_CONTENT_ROOT` names another.
 *
 * @return The data directory; the current directory if the platform cannot tell where the executable is.
 */
std::filesystem::path dataRoot() {
    std::filesystem::path root = engine::contentRoot();
    if (root.empty()) {
        std::error_code error;
        root = std::filesystem::current_path(error);
    }
    return root;
}

/**
 * @brief Loads a level file and reports its warnings.
 * @param path Level file.
 * @param root Data directory the boss scripts are looked up from.
 * @return The level, or `nullptr` if it cannot be loaded (the error is logged).
 */
std::shared_ptr<const rtype::server::level::Level> loadLevelFile(const std::filesystem::path& path,
                                                                 const std::filesystem::path& root) {
    std::vector<std::string> warnings;
    auto level =
        rtype::server::level::loadLevel(path, rtype::server::gameplay::LevelDirectorPlugin::levelPrefab, warnings);
    for (const std::string& warning : warnings) {
        engine::log::warning(rtype::server::LOG_TAG, "level: ", warning);
    }
    if (!level) {
        engine::log::warning(rtype::server::LOG_TAG, "cannot load the level: ", level.error());
        return nullptr;
    }
    level->dataRoot = root;
    engine::log::info(rtype::server::LOG_TAG, "level '", level->name, "' loaded: ", level->timeline.events().size(),
                      " events");
    return std::make_shared<const rtype::server::level::Level>(std::move(*level));
}

/**
 * @brief Loads the level played by every room.
 *
 * A level that cannot be read is not fatal: the server still runs, with no enemy, so that it can start without its data
 * files (from another directory, in a test environment...).
 *
 * @param root Data directory: the level file and the boss scripts are looked up from it.
 * @return The level, or `nullptr` if it cannot be loaded.
 */
std::shared_ptr<const rtype::server::level::Level> loadDefaultLevel(const std::filesystem::path& root) {
    auto level = loadLevelFile(root / LEVEL_PATH, root);
    if (level == nullptr) {
        engine::log::warning(rtype::server::LOG_TAG, "the rooms play without a level");
    }
    return level;
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
    engine::log::setVerbose(options->logs);
    engine::log::setColored(options->colors);

    // The server and room loops sleep 1 ms between two passes: without this, Windows makes it about 15.6 ms.
    const ecs::time::PreciseSleepScope preciseSleep;

    const std::filesystem::path root = dataRoot();
    // A level asked for on the command line must load: playing another one would be misleading.
    const std::shared_ptr<const rtype::server::level::Level> level =
        options->level ? loadLevelFile(*options->level, root) : loadDefaultLevel(root);
    if (options->level && level == nullptr) {
        engine::log::error(rtype::server::LOG_TAG, "the level given with ", rtype::server::LEVEL_OPTION,
                           " cannot be played");
        return EXIT_ERROR;
    }
    rtype::server::ServerManager server(ecs::net::createServer({.protocolId = rtype::protocol::TRANSPORT_PROTOCOL_ID}),
                                        rtype::server::HANDSHAKE_TIMEOUT, level);

    g_server = &server;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    const bool ok = server.run(options->port);
    g_server = nullptr;
    return ok ? 0 : EXIT_ERROR;
}
