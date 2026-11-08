#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
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
 * only scalar values (booleans, integers, floats, strings) are kept, other types (dates, times) are reported in
 * `warnings()`. Holds no library type, so modules reading settings do not depend on the TOML parser.
 *
 * Arrays, including arrays of tables (`[[events]]`), are flattened too: each element is keyed by its index, and
 * `arraySize()` gives the element count.
 *
 * ```toml
 * keys = [10, 20]          # keys.0 = 10, keys.1 = 20, arraySize("keys") = 2
 * [[events]]
 * at = 400.0               # events.0.at
 * [[events]]
 * at = 900.0               # events.1.at, arraySize("events") = 2
 * ```
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

    /** @brief Tells whether `key` holds a scalar value of any type (false for tables and arrays). */
    [[nodiscard]] bool contains(std::string_view key) const;

    /**
     * @brief Gets the element count of an array.
     *
     * Elements are read with their index as the last key part: `getNumber("events.0.at")`.
     *
     * @param key Dotted key of the array, e.g. `"events"` or `"level.waves"`.
     * @return Number of elements, or 0 if `key` is missing, empty or not an array.
     */
    [[nodiscard]] std::size_t arraySize(std::string_view key) const;

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

    /**
     * @brief Sets a value, replacing any value of the same key.
     * @param key Dotted key (`audio.master`); each dot opens a TOML table when saved.
     * @param value New value.
     */
    void set(std::string key, Value value);

    /**
     * @brief Removes a value.
     * @param key Dotted key.
     */
    void erase(std::string_view key);

    /**
     * @brief Gets every value, by dotted key.
     * @return Values sorted by key.
     */
    [[nodiscard]] const std::map<std::string, Value, std::less<>>& values() const noexcept { return _values; }

    /**
     * @brief Writes the values as a TOML document: dotted keys become nested tables.
     * @return TOML text that `parse` reads back to the same values.
     */
    [[nodiscard]] std::string toToml() const;

    /**
     * @brief Writes the values to a file, atomically: a temporary file is written, then renamed over `path`.
     *
     * Missing parent directories are created. A crash during the write never leaves a truncated file.
     * @param path Destination file.
     * @return Nothing on success, or the reason of the failure.
     */
    [[nodiscard]] std::expected<void, std::string> save(const std::filesystem::path& path) const;

    /** @brief Why the file could not be used, if it could not. */
    [[nodiscard]] const std::optional<std::string>& error() const noexcept { return _error; }

    /** @brief Values that were ignored while reading a valid file. */
    [[nodiscard]] const std::vector<std::string>& warnings() const noexcept { return _warnings; }

  private:
    /** @brief Scalar settings by dotted key; `std::less<>` allows lookups by `std::string_view`. */
    std::map<std::string, Value, std::less<>> _values;
    /** @brief Element count of every array by dotted key. */
    std::map<std::string, std::size_t, std::less<>> _arraySizes;
    /** @brief Reason the file could not be used, if any. */
    std::optional<std::string> _error;
    /** @brief Ignored values. */
    std::vector<std::string> _warnings;
};

}  // namespace config
