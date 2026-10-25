#include "config/ConfigReader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "config/ConfigFile.hpp"

namespace config {

ConfigReader::ConfigReader(const ConfigFile& file, std::vector<std::string>& warnings)
    : _file(&file), _warnings(&warnings) {}

float ConfigReader::number(std::string_view key, float fallback, float min, float max) {
    if (!present(key)) {
        return fallback;
    }
    const std::optional<double> value = _file->getNumber(key);
    if (!value || !std::isfinite(*value)) {
        warn(std::format("{}: expected a finite number, using {}", key, fallback));
        return fallback;
    }
    const double clamped = std::clamp(*value, static_cast<double>(min), static_cast<double>(max));
    if (clamped != *value) {
        warn(std::format("{}: {} is out of range [{}, {}], using {}", key, *value, min, max, clamped));
    }
    return static_cast<float>(clamped);
}

std::size_t ConfigReader::count(std::string_view key, std::size_t fallback, std::size_t min, std::size_t max) {
    if (!present(key)) {
        return fallback;
    }
    const std::optional<std::int64_t> value = _file->getInteger(key);
    if (!value) {
        warn(std::format("{}: expected an integer, using {}", key, fallback));
        return fallback;
    }
    const auto clamped = std::clamp(*value, static_cast<std::int64_t>(min), static_cast<std::int64_t>(max));
    if (clamped != *value) {
        warn(std::format("{}: {} is out of range [{}, {}], using {}", key, *value, min, max, clamped));
    }
    return static_cast<std::size_t>(clamped);
}

bool ConfigReader::flag(std::string_view key, bool fallback) {
    if (!present(key)) {
        return fallback;
    }
    const std::optional<bool> value = _file->getBool(key);
    if (!value) {
        warn(std::format("{}: expected true or false, using {}", key, fallback));
        return fallback;
    }
    return *value;
}

std::string ConfigReader::text(std::string_view key, std::string fallback) {
    if (!present(key)) {
        return fallback;
    }
    std::optional<std::string> value = _file->getString(key);
    if (!value) {
        warn(std::format(R"({}: expected a string, using "{}")", key, fallback));
        return fallback;
    }
    return std::move(*value);
}

void ConfigReader::reportUnknownKeys(std::string_view table) {
    for (const std::string& key : _file->keysUnder(table)) {
        if (std::ranges::find(_read, key) == _read.end()) {
            warn(std::format("{}: unknown setting, ignored", key));
        }
    }
}

void ConfigReader::warn(std::string message) { _warnings->push_back(std::move(message)); }

bool ConfigReader::present(std::string_view key) {
    _read.emplace_back(key);
    return _file->contains(key);
}

}  // namespace config
