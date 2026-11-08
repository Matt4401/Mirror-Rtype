/**
 * @file AssetPath.hpp
 * @brief Validation and normalisation of asset paths, aliases and group names.
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace assets {

/** @brief Longest accepted asset path, in bytes. */
inline constexpr std::size_t MAX_PATH_LENGTH = 1024;

/** @brief Longest accepted alias or group name, in bytes. */
inline constexpr std::size_t MAX_NAME_LENGTH = 128;

/** @brief Most files `AssetManager::preloadDirectory` takes from one directory. */
inline constexpr std::size_t MAX_DIRECTORY_FILES = 1024;

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
 * @brief Tells whether a normalised path stays inside the working directory.
 * @param normalized Result of `normalizeAssetPath`.
 * @return False for an absolute path (`/x`, `C:/x`, `//server/x`) or one that climbs out of it (`../x`).
 */
[[nodiscard]] bool isConfinedAssetPath(std::string_view normalized) noexcept;

/**
 * @brief Tells whether a string can be used as a group name.
 * @param name Name to check.
 * @return True for a non-empty name of at most `MAX_NAME_LENGTH` bytes without NUL.
 */
[[nodiscard]] bool isValidAssetName(std::string_view name) noexcept;

/**
 * @brief Tells whether a string can be used as an alias.
 *
 * An alias is a valid name without `/`, `\` or `.`, so that it can never be mistaken for a path (see
 * `looksLikeAssetPath`).
 * @param alias Alias to check.
 * @return True for a valid alias.
 */
[[nodiscard]] bool isValidAssetAlias(std::string_view alias) noexcept;

/**
 * @brief Tells whether a string given where an alias or a path is accepted is a path.
 * @param aliasOrPath Alias or path.
 * @return True if it holds a `/`, a `\` or a `.`; aliases never do.
 */
[[nodiscard]] bool looksLikeAssetPath(std::string_view aliasOrPath) noexcept;

/**
 * @brief Converts a path to UTF-8 with `/` separators, whatever the platform's native encoding.
 *
 * `std::filesystem::path::string()` throws or mangles non-ANSI names on Windows; this does not.
 * @param path Path.
 * @return UTF-8 text.
 */
[[nodiscard]] std::string toUtf8(const std::filesystem::path& path);

/**
 * @brief Cuts a name or path for a log line, so that a script cannot fill the logs with huge strings.
 * @param text Text to show.
 * @param limit Longest part kept, in bytes.
 * @return The text between quotes, ending with `...` when it was cut.
 */
[[nodiscard]] std::string quoteForLog(std::string_view text, std::size_t limit = MAX_NAME_LENGTH);

}  // namespace assets
