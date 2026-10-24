#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace config {

/**
 * @brief Settings read from a TOML file, looked up by dotted key (`"audio.spatial.max_pan"`).
 *
 * Loading never throws: a missing, oversized or malformed file gives an empty configuration and an `error()`,
 * so every lookup misses and callers fall back to their defaults. Nested tables are flattened into dotted keys;
 * only scalar values (booleans, integers, floats, strings) are kept, other types are reported in `warnings()`.
 * Holds no library type, so modules reading settings do not depend on the TOML parser.
 */
class ConfigFile {
  public:
    /** @brief A scalar setting. */
    using Value = std::variant<bool, std::int64_t, double, std::string>;

    /** @brief Creates an empty configuration: every lookup misses. */
    ConfigFile() = default;

    /**
     * @brief Reads and parses a TOML file.
     *
     * @param path File to read.
     * @return The settings, or an empty configuration with `error()` set if the file cannot be used.
     */
    [[nodiscard]] static ConfigFile load(const std::filesystem::path& path);

    /**
     * @brief Parses TOML text.
     *
     * @param text TOML document.
     * @param sourceName Name used in error messages (usually the file path).
     * @return The settings, or an empty configuration with `error()` set if the text is not valid TOML.
     */
    [[nodiscard]] static ConfigFile parse(std::string_view text, std::string_view sourceName = "<memory>");

    /** @brief Tells whether `key` holds a value of any type. */
    [[nodiscard]] bool contains(std::string_view key) const;

    /** @brief Reads a boolean, or nothing if the key is missing or holds another type. */
    [[nodiscard]] std::optional<bool> getBool(std::string_view key) const;

    /** @brief Reads an integer, or nothing if the key is missing or holds another type. */
    [[nodiscard]] std::optional<std::int64_t> getInteger(std::string_view key) const;

    /** @brief Reads an integer or a float as a double, or nothing if the key is missing or holds another type. */
    [[nodiscard]] std::optional<double> getNumber(std::string_view key) const;

    /** @brief Reads a string, or nothing if the key is missing or holds another type. */
    [[nodiscard]] std::optional<std::string> getString(std::string_view key) const;

    /**
     * @brief Lists the keys of a table and of its sub-tables.
     *
     * @param table Dotted table name, e.g. `"audio"`.
     * @return Full dotted keys starting with `table.`, sorted.
     */
    [[nodiscard]] std::vector<std::string> keysUnder(std::string_view table) const;

    /** @brief Why the file could not be used, if it could not. */
    [[nodiscard]] const std::optional<std::string>& error() const noexcept { return _error; }

    /** @brief Values that were ignored while reading a valid file. */
    [[nodiscard]] const std::vector<std::string>& warnings() const noexcept { return _warnings; }

  private:
    /** @brief Scalar settings by dotted key; `std::less<>` allows lookups by `std::string_view`. */
    std::map<std::string, Value, std::less<>> _values;
    /** @brief Reason the file could not be used, if any. */
    std::optional<std::string> _error;
    /** @brief Ignored values. */
    std::vector<std::string> _warnings;
};

}  // namespace config
