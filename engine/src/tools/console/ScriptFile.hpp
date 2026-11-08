/**
 * @file ScriptFile.hpp
 * @brief Reads the script files run by the `exec` console command, confined to a root directory.
 */

#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace tools {

/** @brief Largest script file `exec` reads, in bytes. */
inline constexpr std::size_t MAX_SCRIPT_SIZE = 1024UZ * 1024UZ;

/**
 * @brief Resolves a script path typed in the console inside the script root.
 *
 * The path is untrusted: an absolute path, or one that leaves the root (`..`, a symbolic link pointing outside), is
 * refused, so the console only reads files the program ships with.
 * @param root Script root; empty for the current directory.
 * @param relative Path as typed, relative to the root.
 * @return The resolved path, or an error message for the user.
 */
[[nodiscard]] std::expected<std::filesystem::path, std::string> resolveScriptPath(const std::filesystem::path& root,
                                                                                  std::string_view relative);

/**
 * @brief Reads a script file inside the script root.
 * @param root Script root; empty for the current directory.
 * @param relative Path as typed, relative to the root.
 * @return The content, or an error message if the path is refused, the file is missing, not a regular file, larger
 *         than `MAX_SCRIPT_SIZE` or unreadable.
 */
[[nodiscard]] std::expected<std::string, std::string> readScriptFile(const std::filesystem::path& root,
                                                                     std::string_view relative);

}  // namespace tools
