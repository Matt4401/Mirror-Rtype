#include "tools/cvars/CVarManager.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <expected>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "tools/cvars/CVar.hpp"
#include "tools/cvars/CVarRef.hpp"
#include "tools/cvars/CVarTypes.hpp"

namespace tools {

namespace {

/** @brief Tells whether a byte is an ASCII control character (tabs and newlines included). */
[[nodiscard]] bool isControl(char c) noexcept {
    const auto byte = static_cast<unsigned char>(c);
    return byte < 0x20U || byte == 0x7FU;
}

/** @brief Lowercases ASCII letters. */
[[nodiscard]] std::string toLower(std::string_view text) {
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(),
                           [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; });
    return lower;
}

/** @brief Drops one leading `+`, which `std::from_chars` does not accept. */
[[nodiscard]] std::string_view dropPlus(std::string_view text) noexcept {
    if (text.size() > 1 && text.front() == '+') {
        text.remove_prefix(1);
    }
    return text;
}

[[nodiscard]] std::expected<bool, std::string> parseBool(std::string_view text) {
    const std::string lower = toLower(text);
    if (lower == "1" || lower == "true" || lower == "on" || lower == "yes") {
        return true;
    }
    if (lower == "0" || lower == "false" || lower == "off" || lower == "no") {
        return false;
    }
    return std::unexpected("expected a boolean (true/false, 1/0, on/off)");
}

[[nodiscard]] std::expected<int, std::string> parseInt(std::string_view text) {
    text = dropPlus(text);
    int value = 0;
    const char* begin = std::to_address(text.begin());
    const char* end = std::to_address(text.end());
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec == std::errc::result_out_of_range) {
        return std::unexpected("integer out of range");
    }
    if (ec != std::errc{} || ptr != end) {
        return std::unexpected("expected an integer");
    }
    return value;
}

[[nodiscard]] std::expected<float, std::string> parseFloat(std::string_view text) {
    text = dropPlus(text);
    float value = 0.0F;
    const char* begin = std::to_address(text.begin());
    const char* end = std::to_address(text.end());
    const auto [ptr, ec] = std::from_chars(begin, end, value, std::chars_format::general);
    if (ec == std::errc::result_out_of_range) {
        return std::unexpected("number out of range");
    }
    if (ec != std::errc{} || ptr != end || !std::isfinite(value)) {
        return std::unexpected("expected a number");
    }
    return value;
}

/** @brief Parses text into the alternative held by `like`. */
[[nodiscard]] std::expected<CVarValue, std::string> parseAs(const CVarValue& like, std::string_view text) {
    return std::visit(
        [text]<typename T>(const T& /*unused*/) -> std::expected<CVarValue, std::string> {
            if constexpr (std::is_same_v<T, bool>) {
                return parseBool(text);
            } else if constexpr (std::is_same_v<T, int>) {
                return parseInt(text);
            } else if constexpr (std::is_same_v<T, float>) {
                return parseFloat(text);
            } else {
                return std::string(text);
            }
        },
        like);
}

/** @brief Checks a value against the bounds and string rules of a variable. */
[[nodiscard]] std::expected<void, std::string> validate(const CVarValue& value, const CVarOptions& options) {
    if (const auto* text = std::get_if<std::string>(&value)) {
        if (text->size() > CVarManager::MAX_STRING_LENGTH) {
            return std::unexpected("string longer than " + std::to_string(CVarManager::MAX_STRING_LENGTH) +
                                   " characters");
        }
        if (std::ranges::any_of(*text, isControl)) {
            return std::unexpected(std::string("string contains control characters"));
        }
        return {};
    }
    if (!options.range) {
        return {};
    }
    double number = 0.0;
    if (const auto* integer = std::get_if<int>(&value)) {
        number = static_cast<double>(*integer);
    } else if (const auto* real = std::get_if<float>(&value)) {
        number = static_cast<double>(*real);
    } else {
        return {};
    }
    if (number < options.range->min || number > options.range->max) {
        return std::unexpected("value out of range [" + CVarManager::formatNumber(options.range->min) + ", " +
                               CVarManager::formatNumber(options.range->max) + "]");
    }
    return {};
}

}  // namespace

std::expected<void, std::string> detail::assignCVar(CVarManager& manager, CVar& cvar, CVarValue value) {
    return manager.assign(cvar, std::move(value));
}

CVar& CVarManager::addImpl(CVar&& candidate) {
    const std::string& name = candidate.name();
    if (!isValidName(name)) {
        throw std::invalid_argument("CVarManager: invalid cvar name '" + name + "'");
    }
    if (const auto it = _cvars.find(name); it != _cvars.end()) {
        if (it->second._value.index() != candidate._value.index()) {
            throw std::invalid_argument("CVarManager: cvar '" + name + "' already exists with type " +
                                        std::string(it->second.typeName()));
        }
        return it->second;
    }
    if (auto valid = validate(candidate._default, candidate._options); !valid) {
        throw std::invalid_argument("CVarManager: invalid default for '" + name + "': " + valid.error());
    }
    std::string key = name;
    return _cvars.emplace(std::move(key), std::move(candidate)).first->second;
}

const CVar* CVarManager::find(std::string_view name) const noexcept {
    const auto it = _cvars.find(name);
    return it == _cvars.end() ? nullptr : &it->second;
}

std::expected<void, std::string> CVarManager::assign(CVar& cvar, CVarValue value) {
    if (value.index() != cvar._value.index()) {
        return std::unexpected("'" + cvar._name + "' is a " + std::string(cvar.typeName()));
    }
    if (auto valid = validate(value, cvar._options); !valid) {
        return std::unexpected("'" + cvar._name + "': " + valid.error());
    }
    store(cvar, std::move(value));
    return {};
}

void CVarManager::store(CVar& cvar, CVarValue value) {
    if (value == cvar._value) {
        return;
    }
    cvar._value = std::move(value);
    if (std::ranges::find(_changed, cvar._name) == _changed.end()) {
        _changed.push_back(cvar._name);
    }
}

std::expected<CVar*, std::string> CVarManager::findWritable(std::string_view name) {
    const auto it = _cvars.find(name);
    if (it == _cvars.end()) {
        return std::unexpected("Unknown cvar '" + std::string(name) + "'");
    }
    CVar& cvar = it->second;
    if (hasFlag(cvar.flags(), CVarFlag::ReadOnly)) {
        return std::unexpected("'" + cvar._name + "' is read-only");
    }
    if (hasFlag(cvar.flags(), CVarFlag::Cheat) && !_cheatsAllowed) {
        return std::unexpected("'" + cvar._name + "' is cheat-protected and cheats are disabled");
    }
    return &cvar;
}

std::expected<void, std::string> CVarManager::setFromString(std::string_view name, std::string_view text) {
    auto found = findWritable(name);
    if (!found) {
        return std::unexpected(std::move(found.error()));
    }
    CVar& cvar = **found;
    auto parsed = parseAs(cvar._value, text);
    if (!parsed) {
        return std::unexpected("'" + cvar._name + "': " + parsed.error());
    }
    return assign(cvar, std::move(*parsed));
}

std::expected<void, std::string> CVarManager::reset(std::string_view name) {
    auto found = findWritable(name);
    if (!found) {
        return std::unexpected(std::move(found.error()));
    }
    CVar& cvar = **found;
    store(cvar, cvar._default);
    return {};
}

void CVarManager::setCheatsAllowed(bool allowed) {
    _cheatsAllowed = allowed;
    if (allowed) {
        return;
    }
    for (auto& [name, cvar] : _cvars) {
        if (hasFlag(cvar.flags(), CVarFlag::Cheat)) {
            store(cvar, cvar._default);
        }
    }
}

std::vector<const CVar*> CVarManager::list(std::string_view prefix) const {
    std::vector<const CVar*> found;
    for (auto it = _cvars.lower_bound(prefix); it != _cvars.end() && it->first.starts_with(prefix); ++it) {
        found.push_back(&it->second);
    }
    return found;
}

void CVarManager::drainChanged(const std::function<void(std::string_view)>& callback) {
    const std::vector<std::string> changed = std::exchange(_changed, {});
    for (const std::string& name : changed) {
        callback(name);
    }
}

bool CVarManager::isValidName(std::string_view name) noexcept {
    if (name.empty() || name.size() > MAX_NAME_LENGTH) {
        return false;
    }
    const auto isLowerOrUnderscore = [](char c) { return (c >= 'a' && c <= 'z') || c == '_'; };
    if (!isLowerOrUnderscore(name.front())) {
        return false;
    }
    return std::ranges::all_of(
        name, [&isLowerOrUnderscore](char c) { return isLowerOrUnderscore(c) || (c >= '0' && c <= '9') || c == '.'; });
}

std::string CVarManager::formatNumber(double number) {
    std::array<char, 32> buffer{};
    const auto [ptr, ec] = std::to_chars(buffer.data(), std::to_address(buffer.end()), number);
    return ec == std::errc{} ? std::string(buffer.data(), ptr) : std::string("?");
}

std::string CVarManager::format(const CVarValue& value) {
    return std::visit(
        []<typename T>(const T& held) -> std::string {
            if constexpr (std::is_same_v<T, bool>) {
                return held ? "true" : "false";
            } else if constexpr (std::is_same_v<T, int>) {
                return std::to_string(held);
            } else if constexpr (std::is_same_v<T, float>) {
                std::array<char, 32> buffer{};
                const auto [ptr, ec] = std::to_chars(buffer.data(), std::to_address(buffer.end()), held);
                return ec == std::errc{} ? std::string(buffer.data(), ptr) : std::string("?");
            } else {
                return held;
            }
        },
        value);
}

}  // namespace tools
