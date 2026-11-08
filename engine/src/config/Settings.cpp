#include "config/Settings.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "config/ConfigFile.hpp"

namespace config {

namespace {

/**
 * @brief Reads an environment variable.
 * @param name Variable name.
 * @return Its value, or nothing if it is unset or empty.
 */
std::optional<std::filesystem::path> environment(const char* name) {
    // NOLINTNEXTLINE(concurrency-mt-unsafe): read once at start-up, never written by the engine.
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }
    return std::filesystem::path(value);
}

}  // namespace

Settings::Settings(std::filesystem::path file) : _file(std::move(file)) {}

std::optional<Settings::Value> Settings::convert(const Entry& entry, const Value& value) {
    if (entry.fallback.index() == value.index()) {
        return value;
    }
    if (std::holds_alternative<double>(entry.fallback) && std::holds_alternative<std::int64_t>(value)) {
        return Value{static_cast<double>(std::get<std::int64_t>(value))};
    }
    return std::nullopt;
}

void Settings::define(std::string key, Value defaultValue) {
    Entry declared{.value = defaultValue, .fallback = std::move(defaultValue)};
    if (const auto it = _entries.find(key); it != _entries.end()) {
        if (const auto kept = convert(declared, it->second.value)) {
            declared.value = *kept;
        }
    } else if (const auto loaded = _loaded.values().find(key); loaded != _loaded.values().end()) {
        if (const auto value = convert(declared, loaded->second)) {
            declared.value = *value;
        } else {
            _warnings.push_back(std::format("{}: value of the wrong type, the default is used", key));
        }
    }
    _entries.insert_or_assign(std::move(key), std::move(declared));
}

bool Settings::has(std::string_view key) const noexcept { return _entries.contains(key); }

bool Settings::set(std::string_view key, const Value& value) {
    const auto it = _entries.find(key);
    if (it == _entries.end()) {
        return false;
    }
    const std::optional<Value> converted = convert(it->second, value);
    if (!converted || *converted == it->second.value) {
        return false;
    }
    it->second.value = *converted;
    markChanged(key);
    return true;
}

bool Settings::reset(std::string_view key) {
    const auto it = _entries.find(key);
    return it != _entries.end() && set(key, it->second.fallback);
}

const Settings::Entry& Settings::entry(std::string_view key) const {
    const auto it = _entries.find(key);
    if (it == _entries.end()) {
        throw std::out_of_range(std::format("Settings: '{}' is not declared", key));
    }
    return it->second;
}

bool Settings::getBool(std::string_view key) const {
    const auto* value = std::get_if<bool>(&entry(key).value);
    if (value == nullptr) {
        throw std::out_of_range(std::format("Settings: '{}' is not a boolean", key));
    }
    return *value;
}

std::int64_t Settings::getInteger(std::string_view key) const {
    const auto* value = std::get_if<std::int64_t>(&entry(key).value);
    if (value == nullptr) {
        throw std::out_of_range(std::format("Settings: '{}' is not an integer", key));
    }
    return *value;
}

double Settings::getNumber(std::string_view key) const {
    const Value& value = entry(key).value;
    if (const auto* number = std::get_if<double>(&value)) {
        return *number;
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        return static_cast<double>(*integer);
    }
    throw std::out_of_range(std::format("Settings: '{}' is not a number", key));
}

const std::string& Settings::getString(std::string_view key) const {
    const auto* value = std::get_if<std::string>(&entry(key).value);
    if (value == nullptr) {
        throw std::out_of_range(std::format("Settings: '{}' is not a string", key));
    }
    return *value;
}

std::expected<void, std::string> Settings::load() {
    _warnings.clear();
    std::error_code error;
    if (_file.empty() || !std::filesystem::exists(_file, error)) {
        return {};
    }
    ConfigFile file = ConfigFile::load(_file);
    if (file.error()) {
        return std::unexpected(*file.error());
    }
    _warnings = file.warnings();
    _loaded = std::move(file);
    for (auto& [key, declared] : _entries) {
        const auto loaded = _loaded.values().find(key);
        if (loaded == _loaded.values().end()) {
            continue;
        }
        if (const auto value = convert(declared, loaded->second)) {
            if (*value != declared.value) {
                declared.value = *value;
                markChanged(key);
            }
        } else {
            _warnings.push_back(std::format("{}: value of the wrong type, the default is used", key));
        }
    }
    _dirty = false;
    return {};
}

std::expected<void, std::string> Settings::save() {
    if (_file.empty()) {
        _dirty = false;
        return {};
    }
    ConfigFile out = _loaded;
    for (const auto& [key, declared] : _entries) {
        out.set(key, declared.value);
    }
    auto saved = out.save(_file);
    if (saved) {
        _loaded = std::move(out);
        _dirty = false;
    }
    return saved;
}

void Settings::drainChanged(const std::function<void(std::string_view)>& visit) {
    // Swapped out first, so a visitor changing a setting records it for the next drain.
    std::vector<std::string> changed;
    changed.swap(_changed);
    for (const std::string& key : changed) {
        visit(key);
    }
    changed.clear();
    if (_changed.empty()) {
        _changed.swap(changed);  // keep the capacity
    }
}

void Settings::markChanged(std::string_view key) {
    _dirty = true;
    if (std::ranges::find(_changed, key) == _changed.end()) {
        _changed.emplace_back(key);
    }
}

std::filesystem::path userConfigDirectory(std::string_view appName) {
    const std::filesystem::path app{std::string(appName)};
#ifdef _WIN32
    if (const auto appData = environment("APPDATA")) {
        return *appData / app;
    }
#elif defined(__APPLE__)  // NOLINT(readability-use-concise-preprocessor-directives): #elifdef is C++23 only
    if (const auto home = environment("HOME")) {
        return *home / "Library" / "Application Support" / app;
    }
#else
    if (const auto xdg = environment("XDG_CONFIG_HOME")) {
        return *xdg / app;
    }
    if (const auto home = environment("HOME")) {
        return *home / ".config" / app;
    }
#endif
    std::error_code error;
    return std::filesystem::current_path(error) / app;
}

}  // namespace config
