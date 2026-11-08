#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace systems::detail {

/**
 * @brief Turns a Wren error (`WrenVM::lastError()` or an `interpret` error) into one log line.
 *
 * The message and its stack trace lines are trimmed and joined with ` | `:
 * `Runtime Error: Null does not implement 'x' | in scripts/Boss line 31 in update(_)`.
 * @param error Error text, one line per `\n`.
 * @return The line, or "unknown error" for an empty text.
 */
[[nodiscard]] std::string formatScriptError(std::string_view error);

/**
 * @brief Turns a script module name into the file to read, under the script root.
 *
 * Refused (not logged, the caller does): empty, absolute or drive names, and names climbing out of the root with `..`.
 * @param root Script root; empty for the working directory.
 * @param moduleName Module name, with or without `.wren` (e.g. `scripts/Boss`).
 * @return The file, or nothing for a refused name.
 */
[[nodiscard]] std::optional<std::filesystem::path> resolveScriptFile(const std::filesystem::path& root,
                                                                     std::string_view moduleName);

/**
 * @brief Logs that a script module name was refused by `resolveScriptFile`.
 * @param moduleName Refused name, cut in the message if long.
 */
void logRefusedScriptModule(std::string_view moduleName);

}  // namespace systems::detail
