/**
 * @file Settings.hpp
 * @brief Typed user settings with defaults, persisted to a TOML file.
 */

#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "config/ConfigFile.hpp"

namespace config {

/**
 * @brief User settings: each key is declared with a default value that fixes its type, then read and changed.
 *
 * Values are loaded from a TOML file (dotted keys are nested tables) and saved back to it. A file value of the wrong
 * type is ignored with a warning, and keys of the file that the game does not declare are kept untouched on save, so
 * an older or newer version of the game does not erase them. `define` may come before or after `load`.
 *
 * A number setting declared with a `double` default accepts integers too. Reading an undeclared key is a programming
 * error and throws `std::out_of_range`.
 *
 * Changes are recorded: `dirty()` tells a save is due, and `drainChanged` hands the changed keys over once.
 */
class Settings {
  public:
    /** @brief Value of a setting: the scalar types of `ConfigFile`. */
    using Value = ConfigFile::Value;

    /**
     * @brief Creates empty settings bound to a file.
     * @param file File read by `load` and written by `save`; empty for settings kept in memory only.
     */
    explicit Settings(std::filesystem::path file = {});

    /**
     * @brief Declares a setting.
     *
     * If a value of the right type was loaded for this key, it is used; otherwise the setting starts at its default.
     * Declaring a key again replaces its default and keeps its current value if the type matches.
     * @param key Dotted key (`audio.master`).
     * @param defaultValue Default value; its type is the type of the setting.
     */
    void define(std::string key, Value defaultValue);

    /**
     * @brief Tells whether a setting is declared.
     * @param key Dotted key.
     * @return `true` if `define` was called for it.
     */
    [[nodiscard]] bool has(std::string_view key) const noexcept;

    /**
     * @brief Changes a setting.
     * @param key Declared key.
     * @param value New value, of the setting's type (an integer is accepted for a number setting).
     * @return `true` if the value changed; `false` if it was already this value, the key is not declared, or the type
     *         does not match.
     */
    bool set(std::string_view key, const Value& value);

    /**
     * @brief Resets a setting to its default.
     * @param key Declared key.
     * @return `true` if the value changed.
     */
    bool reset(std::string_view key);

    /**
     * @brief Reads a boolean setting.
     * @param key Declared key of type `bool`.
     * @return Its value.
     * @throws std::out_of_range if the key is not declared or not a boolean.
     */
    [[nodiscard]] bool getBool(std::string_view key) const;

    /**
     * @brief Reads an integer setting.
     * @param key Declared key of type `std::int64_t`.
     * @return Its value.
     * @throws std::out_of_range if the key is not declared or not an integer.
     */
    [[nodiscard]] std::int64_t getInteger(std::string_view key) const;

    /**
     * @brief Reads a number setting.
     * @param key Declared key of type `double` (or `std::int64_t`, converted).
     * @return Its value.
     * @throws std::out_of_range if the key is not declared or not a number.
     */
    [[nodiscard]] double getNumber(std::string_view key) const;

    /**
     * @brief Reads a string setting.
     * @param key Declared key of type `std::string`.
     * @return Its value, valid until the setting changes.
     * @throws std::out_of_range if the key is not declared or not a string.
     */
    [[nodiscard]] const std::string& getString(std::string_view key) const;

    /**
     * @brief Reads the file given at construction; a missing file is not an error (first launch).
     * @return Nothing on success or for a missing file, or the parse error (the defaults are kept).
     */
    std::expected<void, std::string> load();

    /**
     * @brief Writes the settings and the unknown keys of the loaded file, atomically, and clears `dirty()`.
     * @return Nothing on success (or without a file), or the reason of the failure.
     */
    std::expected<void, std::string> save();

    /**
     * @brief Tells whether a setting changed since the last load or save.
     * @return `true` if a save is due.
     */
    [[nodiscard]] bool dirty() const noexcept { return _dirty; }

    /**
     * @brief Calls `visit` for each key changed since the previous call, then forgets them.
     * @param visit Called with each changed key, once per key.
     */
    void drainChanged(const std::function<void(std::string_view)>& visit);

    /**
     * @brief Gets the file of the settings.
     * @return Path given at construction.
     */
    [[nodiscard]] const std::filesystem::path& file() const noexcept { return _file; }

    /**
     * @brief Gets the warnings of the last load (wrong types, unsupported values).
     * @return Warning messages.
     */
    [[nodiscard]] const std::vector<std::string>& warnings() const noexcept { return _warnings; }

  private:
    /** @brief A declared setting. */
    struct Entry {
        /** @brief Current value. */
        Value value;
        /** @brief Default value, which fixes the type. */
        Value fallback;
    };

    /**
     * @brief Converts a value to the type of a setting.
     * @param entry Setting.
     * @param value Candidate value.
     * @return The converted value, or nothing if the types do not match.
     */
    [[nodiscard]] static std::optional<Value> convert(const Entry& entry, const Value& value);

    /**
     * @brief Gets a declared setting.
     * @param key Dotted key.
     * @return The setting.
     * @throws std::out_of_range if it is not declared.
     */
    [[nodiscard]] const Entry& entry(std::string_view key) const;

    /**
     * @brief Records a changed key.
     * @param key Key that changed.
     */
    void markChanged(std::string_view key);

    /** @brief File read and written. */
    std::filesystem::path _file;
    /** @brief Declared settings, by key. */
    std::map<std::string, Entry, std::less<>> _entries;
    /** @brief Values of the loaded file, including the keys not declared. */
    ConfigFile _loaded;
    /** @brief Keys changed since the last `drainChanged`. */
    std::vector<std::string> _changed;
    /** @brief Warnings of the last load. */
    std::vector<std::string> _warnings;
    /** @brief A save is due. */
    bool _dirty = false;
};

/**
 * @brief Gets the per-user directory where an application stores its settings.
 *
 * `%APPDATA%/<app>` on Windows, `~/Library/Application Support/<app>` on macOS, `$XDG_CONFIG_HOME/<app>` (else
 * `~/.config/<app>`) elsewhere. Falls back to `<current directory>/<app>` when no home is known.
 * @param appName Directory name of the application.
 * @return Directory path; it may not exist yet (`ConfigFile::save` creates it).
 */
[[nodiscard]] std::filesystem::path userConfigDirectory(std::string_view appName);

}  // namespace config
