/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Server entry point
*/

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "LogTag.hpp"
#include "Logger.hpp"
#include "ServerManager.hpp"
#include "ecs/time/PreciseSleep.hpp"
#include "gameplay/LevelDirectorPlugin.hpp"
#include "level/Level.hpp"
#include "level/LevelLoader.hpp"
#include "net/NetworkFactory.hpp"
#include "parsing/Parsing.hpp"
#include "protocol/Messages.hpp"

namespace {
constexpr int EXIT_ERROR = 84;
/** @brief Directory of the campaign levels, relative to the data directory (see `dataRoot()`). */
constexpr const char* LEVELS_DIRECTORY = "server/levels";
/** @brief Name prefix of a campaign level file; the levels are played in the order of their names. */
constexpr std::string_view LEVEL_PREFIX = "level_";
/** @brief Extension of a level file. */
constexpr std::string_view LEVEL_EXTENSION = ".toml";

// The signal handler cannot take arguments, so it reaches the server through a global.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<rtype::server::ServerManager*> g_server{nullptr};

/**
 * @brief Finds the directory holding the server data (`server/levels/`, `scripts/`): the directory of the executable,
 * which the build puts at the project root.
 *
 * Resolved from `argv[0]`, which works on every platform when the server is started through a path. A bare name found
 * through `PATH` has no directory: the current directory is used then.
 *
 * @param program `argv[0]`.
 * @return The data directory.
 */
std::filesystem::path dataRoot(const char* program) {
    std::error_code error;
    const std::filesystem::path executable(program != nullptr ? program : "");
    if (!executable.has_parent_path()) {
        return std::filesystem::current_path(error);
    }
    const std::filesystem::path directory = std::filesystem::absolute(executable, error).parent_path();
    return error ? std::filesystem::current_path(error) : directory;
}

/**
 * @brief Loads a level file and reports its warnings.
 * @param path Level file.
 * @param root Data directory the boss scripts are looked up from.
 * @return The level, or nothing if it cannot be loaded (the error is logged).
 */
std::optional<rtype::server::level::Level> loadLevelFile(const std::filesystem::path& path,
                                                         const std::filesystem::path& root) {
    std::vector<std::string> warnings;
    auto level =
        rtype::server::level::loadLevel(path, rtype::server::gameplay::LevelDirectorPlugin::levelPrefab, warnings);
    for (const std::string& warning : warnings) {
        rtype::log::warning(rtype::server::LOG_TAG, "level: ", warning);
    }
    if (!level) {
        rtype::log::warning(rtype::server::LOG_TAG, "cannot load the level ", path.string(), ": ", level.error());
        return std::nullopt;
    }
    level->dataRoot = root;
    rtype::log::info(rtype::server::LOG_TAG, "level '", level->name, "' loaded: ", level->timeline.events().size(),
                     " events");
    return std::move(*level);
}

/**
 * @brief Loads the campaign played by every room: the `level_*.toml` files of `server/levels/`, by name, chained
 * through `Level::next`.
 *
 * A level that cannot be read is skipped; no level at all is not fatal either: the server still runs, with no enemy, so
 * that it can start without its data files (from another directory, in a test environment...).
 *
 * @param root Data directory: the level files and the boss scripts are looked up from it.
 * @return The first level, or `nullptr` if none can be loaded.
 */
std::shared_ptr<const rtype::server::level::Level> loadCampaign(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> files;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(root / LEVELS_DIRECTORY, error)) {
        const std::string name = entry.path().filename().string();
        if (entry.is_regular_file(error) && name.starts_with(LEVEL_PREFIX) && name.ends_with(LEVEL_EXTENSION)) {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    std::vector<rtype::server::level::Level> levels;
    for (const std::filesystem::path& file : files) {
        if (auto level = loadLevelFile(file, root)) {
            levels.push_back(std::move(*level));
        }
    }
    // Chained from the last one, so that each level can point to the already built next one.
    std::shared_ptr<const rtype::server::level::Level> next;
    for (auto& level : std::ranges::reverse_view(levels)) {
        level.next = next;
        next = std::make_shared<const rtype::server::level::Level>(std::move(level));
    }
    if (next == nullptr) {
        rtype::log::warning(rtype::server::LOG_TAG, "no level found in ", (root / LEVELS_DIRECTORY).string(),
                            ": the rooms play without a level");
    } else {
        rtype::log::info(rtype::server::LOG_TAG, "campaign of ", rtype::server::level::campaignLength(next.get()),
                         " level(s)");
    }
    return next;
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

    // The server and room loops sleep 1 ms between two passes: without this, Windows makes it about 15.6 ms.
    const ecs::time::PreciseSleepScope preciseSleep;

    const std::filesystem::path root = dataRoot(args.front());
    // A level asked for on the command line is played alone, and must load: playing another one would be misleading.
    std::shared_ptr<const rtype::server::level::Level> level;
    if (options->level) {
        if (auto single = loadLevelFile(*options->level, root)) {
            level = std::make_shared<const rtype::server::level::Level>(std::move(*single));
        }
    } else {
        level = loadCampaign(root);
    }
    if (options->level && level == nullptr) {
        rtype::log::error(rtype::server::LOG_TAG, "the level given with ", rtype::server::LEVEL_OPTION,
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
