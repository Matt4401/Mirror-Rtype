/**
 * @file AssetPath.hpp
 * @brief Validation and normalisation of asset paths, aliases and group names.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace assets {

/** @brief Longest accepted asset path, in bytes. */
inline constexpr std::size_t MAX_PATH_LENGTH = 1024;

/** @brief Longest accepted alias or group name, in bytes. */
inline constexpr std::size_t MAX_NAME_LENGTH = 128;

/**
 * @brief Turns a path into the key the manager stores it under.
 *
 * Both `/` and `\` are separators, `.` and `..` are folded (`a/../a/x.png` is `a/x.png`) and the result uses `/`, so
 * the same file gets the same key on Linux and Windows. The path is not resolved against the disk.
 *
 * @param path Path given by the game or a script.
 * @return The key, or nothing for an empty, too long or NUL-holding path.
 */
[[nodiscard]] std::optional<std::string> normalizeAssetPath(std::string_view path);

/**
 * @brief Tells whether a string can be used as an alias or a group name.
 * @param name Name to check.
 * @return True for a non-empty name of at most `MAX_NAME_LENGTH` bytes without NUL.
 */
[[nodiscard]] bool isValidAssetName(std::string_view name) noexcept;

}  // namespace assets
