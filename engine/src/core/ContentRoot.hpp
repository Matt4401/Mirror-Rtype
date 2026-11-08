/**
 * @file ContentRoot.hpp
 * @brief Directory the game's relative content paths (scripts, assets, levels) are resolved against.
 */

#pragma once

#include <filesystem>
#include <string_view>

namespace engine {

/** @brief Environment variable overriding the content root (a game installed apart from its content, tests...). */
inline constexpr std::string_view CONTENT_ROOT_VARIABLE = "ENGINE_CONTENT_ROOT";

/**
 * @brief Gets the directory holding the running executable.
 * @return Its absolute path, or an empty path if the platform cannot tell.
 */
[[nodiscard]] std::filesystem::path executableDirectory();

/**
 * @brief Gets the directory a game resolves its relative content paths against, so that it runs from any working
 * directory.
 *
 * In order: the `ENGINE_CONTENT_ROOT` environment variable if it names a directory, then the directory of the
 * executable, then an empty path (the working directory, as before).
 *
 * @return The content root, or an empty path.
 */
[[nodiscard]] std::filesystem::path contentRoot();

/**
 * @brief Resolves a content path against a root.
 * @param root Content root; empty for the working directory.
 * @param path Path; an absolute one is returned unchanged.
 * @return `root / path`, or `path` if the root is empty or the path absolute.
 */
[[nodiscard]] std::filesystem::path resolveContentPath(const std::filesystem::path& root,
                                                       const std::filesystem::path& path);

}  // namespace engine
