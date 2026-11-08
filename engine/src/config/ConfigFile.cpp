#include "config/ConfigFile.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <ios>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <toml++/toml.hpp>  // IWYU pragma: keep
#include <utility>
#include <variant>
#include <vector>

#include "config/ConfigConstants.hpp"

namespace config {

namespace {

using Values = std::map<std::string, ConfigFile::Value, std::less<>>;

/** @brief Copies the scalar values of `root` and of its sub-tables into `values`, under dotted keys. */
void flatten(const toml::table& root, Values& values, std::vector<std::string>& warnings) {
    // Sub-tables are queued with their dotted prefix instead of recursing, whatever the file nesting.
    std::vector<std::pair<const toml::table*, std::string>> pending{{&root, std::string{}}};
    while (!pending.empty()) {
        const auto [table, prefix] = std::move(pending.back());
        pending.pop_back();
        for (const auto& [key, node] : *table) {
            std::string name = prefix.empty() ? std::string(key.str()) : std::format("{}.{}", prefix, key.str());
            if (const toml::table* child = node.as_table()) {
                pending.emplace_back(child, std::move(name));
            } else if (const auto* boolean = node.as_boolean()) {
                values.emplace(name, boolean->get());
            } else if (const auto* integer = node.as_integer()) {
                values.emplace(name, integer->get());
            } else if (const auto* floating = node.as_floating_point()) {
                values.emplace(name, floating->get());
            } else if (const auto* string = node.as_string()) {
                values.emplace(name, string->get());
            } else {
                warnings.push_back(std::format("{}: unsupported value type (arrays and dates are ignored)", name));
            }
        }
    }
}

/** @brief Returns the value of `key` if it holds a `T`, or nullptr. */
template <typename T>
const T* find(const Values& values, std::string_view key) {
    const auto it = values.find(key);
    return it == values.end() ? nullptr : std::get_if<T>(&it->second);
}

}  // namespace

ConfigFile ConfigFile::load(const std::filesystem::path& path) {
    ConfigFile file;
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        file._error = std::format("{}: cannot read file ({})", path.string(), error.message());
        return file;
    }
    if (size > MAX_CONFIG_FILE_SIZE) {
        file._error =
            std::format("{}: file is too large ({} bytes, at most {})", path.string(), size, MAX_CONFIG_FILE_SIZE);
        return file;
    }

    std::ifstream stream(path, std::ios::binary);
    std::string text(static_cast<std::size_t>(size), '\0');
    if (!stream || !stream.read(text.data(), static_cast<std::streamsize>(size))) {
        file._error = std::format("{}: cannot read file", path.string());
        return file;
    }
    return parse(text, path.string());
}

ConfigFile ConfigFile::parse(std::string_view text, std::string_view sourceName) {
    ConfigFile file;
    try {
        const toml::table table = toml::parse(text, sourceName);
        flatten(table, file._values, file._warnings);
    } catch (const toml::parse_error& parseError) {
        const toml::source_position& where = parseError.source().begin;
        file._error = std::format("{}:{}:{}: {}", sourceName, where.line, where.column, parseError.description());
    }
    return file;
}

bool ConfigFile::contains(std::string_view key) const { return _values.contains(key); }

std::optional<bool> ConfigFile::getBool(std::string_view key) const {
    const auto* value = find<bool>(_values, key);
    return value != nullptr ? std::optional<bool>(*value) : std::nullopt;
}

std::optional<std::int64_t> ConfigFile::getInteger(std::string_view key) const {
    const auto* value = find<std::int64_t>(_values, key);
    return value != nullptr ? std::optional<std::int64_t>(*value) : std::nullopt;
}

std::optional<double> ConfigFile::getNumber(std::string_view key) const {
    if (const auto* value = find<double>(_values, key); value != nullptr) {
        return *value;
    }
    if (const auto* value = find<std::int64_t>(_values, key); value != nullptr) {
        return static_cast<double>(*value);
    }
    return std::nullopt;
}

std::optional<std::string> ConfigFile::getString(std::string_view key) const {
    const auto* value = find<std::string>(_values, key);
    return value != nullptr ? std::optional<std::string>(*value) : std::nullopt;
}

std::vector<std::string> ConfigFile::keysUnder(std::string_view table) const {
    const std::string prefix = std::format("{}.", table);
    std::vector<std::string> keys;
    for (auto it = _values.lower_bound(prefix); it != _values.end() && it->first.starts_with(prefix); ++it) {
        keys.push_back(it->first);
    }
    return keys;
}

void ConfigFile::set(std::string key, Value value) { _values.insert_or_assign(std::move(key), std::move(value)); }

void ConfigFile::erase(std::string_view key) {
    if (const auto it = _values.find(key); it != _values.end()) {
        _values.erase(it);
    }
}

std::string ConfigFile::toToml() const {
    toml::table root;
    for (const auto& [key, value] : _values) {
        toml::table* table = &root;
        std::string_view rest = key;
        for (std::size_t dot = rest.find('.'); dot != std::string_view::npos; dot = rest.find('.')) {
            const std::string_view part = rest.substr(0, dot);
            rest.remove_prefix(dot + 1);
            toml::node* child = table->get(part);
            if (child == nullptr || !child->is_table()) {
                table->insert_or_assign(part, toml::table{});
                child = table->get(part);
            }
            table = child->as_table();
        }
        std::visit([&](const auto& scalar) { table->insert_or_assign(rest, scalar); }, value);
    }
    std::ostringstream out;
    out << root << '\n';
    return out.str();
}

std::expected<void, std::string> ConfigFile::save(const std::filesystem::path& path) const {
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return std::unexpected(std::format("{}: cannot create directory ({})", path.string(), error.message()));
        }
    }
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        const std::string text = toToml();
        if (!stream || !stream.write(text.data(), static_cast<std::streamsize>(text.size())) || !stream.flush()) {
            return std::unexpected(std::format("{}: cannot write file", temporary.string()));
        }
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        const std::string reason = error.message();
        std::filesystem::remove(temporary, error);
        return std::unexpected(std::format("{}: cannot replace file ({})", path.string(), reason));
    }
    return {};
}

}  // namespace config
