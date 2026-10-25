#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "config/ConfigFile.hpp"

namespace config {

/**
 * @brief Reads typed, bounded settings from a `ConfigFile` and reports every correction.
 *
 * Each read returns a usable value: a missing key gives the fallback silently, while a value of the wrong type,
 * out of range or not finite is replaced or clamped and reported in the warnings. Every key read is remembered, so
 * `reportUnknownKeys` can flag typos. Modules use it to turn their table of the file into their own settings.
 *
 * The reader keeps pointers to the file and the warnings: both must outlive it.
 */
class ConfigReader {
  public:
    /**
     * @brief Prepares to read `file`.
     *
     * @param file Parsed configuration.
     * @param warnings Receives one message per ignored or corrected value.
     */
    ConfigReader(const ConfigFile& file, std::vector<std::string>& warnings);

    /**
     * @brief Reads an integer or float.
     *
     * @param key Dotted key.
     * @param fallback Value used when the key is missing or invalid.
     * @param min Lowest accepted value.
     * @param max Highest accepted value.
     * @return The value clamped to [min, max], or `fallback`.
     */
    [[nodiscard]] float number(std::string_view key, float fallback, float min, float max);

    /**
     * @brief Reads a non-negative integer.
     *
     * @param key Dotted key.
     * @param fallback Value used when the key is missing or invalid.
     * @param min Lowest accepted value.
     * @param max Highest accepted value.
     * @return The value clamped to [min, max], or `fallback`.
     */
    [[nodiscard]] std::size_t count(std::string_view key, std::size_t fallback, std::size_t min, std::size_t max);

    /**
     * @brief Reads a boolean.
     *
     * @param key Dotted key.
     * @param fallback Value used when the key is missing or invalid.
     * @return The value, or `fallback`.
     */
    [[nodiscard]] bool flag(std::string_view key, bool fallback);

    /**
     * @brief Reads a string; checking it against the accepted values is up to the caller.
     *
     * @param key Dotted key.
     * @param fallback Value used when the key is missing or invalid.
     * @return The value, or `fallback`.
     */
    [[nodiscard]] std::string text(std::string_view key, std::string fallback);

    /**
     * @brief Reports the keys of `table` and its sub-tables that were never read, usually typos.
     * @param table Dotted table name, e.g. `"audio"`.
     */
    void reportUnknownKeys(std::string_view table);

    /**
     * @brief Adds a message to the warnings, e.g. for a value the caller rejects.
     * @param message Message, starting with the key it is about.
     */
    void warn(std::string message);

  private:
    /** @brief Records `key` as known and tells whether the file sets it. */
    [[nodiscard]] bool present(std::string_view key);

    /** @brief Configuration being read; never null. */
    const ConfigFile* _file;
    /** @brief Destination of the messages; never null. */
    std::vector<std::string>* _warnings;
    /** @brief Keys read so far. */
    std::vector<std::string> _read;
};

}  // namespace config
