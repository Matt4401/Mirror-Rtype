#include "input/KeyboardLayout.hpp"

#include <array>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace input {

namespace detail {

bool isAzertyLocale(std::string_view locale) noexcept {
    if (locale.starts_with("fr_CA") || locale.starts_with("fr-CA")) {
        return false;
    }
    return locale.starts_with("fr_FR") || locale.starts_with("fr_BE") || locale.starts_with("fr-FR") ||
           locale.starts_with("fr-BE") || locale.starts_with("fr_") || locale.starts_with("fr-");
}

std::optional<KeyboardLayout> detectFromLinuxConfig() noexcept {
#ifdef __linux__
    std::ifstream file("/etc/default/keyboard");
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::string_view sv(line);
        if (sv.starts_with("XKBLAYOUT=")) {
            sv.remove_prefix(10);
            if (!sv.empty() && (sv.front() == '"' || sv.front() == '\'')) {
                sv.remove_prefix(1);
            }
            if (!sv.empty() && (sv.back() == '"' || sv.back() == '\'')) {
                sv.remove_suffix(1);
            }
            if (sv.starts_with("fr") || sv.starts_with("be")) {
                return KeyboardLayout::Azerty;
            }
            return KeyboardLayout::Qwerty;
        }
    }
#endif
    return std::nullopt;
}

std::optional<KeyboardLayout> detectFromPlatform() noexcept {
#ifdef _WIN32
    const HKL layout = GetKeyboardLayout(0);
    const WORD langId = LOWORD(layout);
    if (langId == WIN_LANGID_FRENCH_FRANCE || langId == WIN_LANGID_FRENCH_BELGIUM) {
        return KeyboardLayout::Azerty;
    }
    return KeyboardLayout::Qwerty;
#elifdef __linux__
    return detectFromLinuxConfig();
#else
    return std::nullopt;
#endif
}

std::optional<KeyboardLayout> detectFromEnvironment() noexcept {
    constexpr std::array<const char*, 4> ENV_VARS = {"LC_ALL", "LC_CTYPE", "LC_MESSAGES", "LANG"};
    for (const char* const envName : ENV_VARS) {
        const char* val = std::getenv(envName);
        if (val != nullptr) {
            const std::string_view locale(val);
            if (!locale.empty() && isAzertyLocale(locale)) {
                return KeyboardLayout::Azerty;
            }
        }
    }
    return std::nullopt;
}

}  // namespace detail

KeyboardLayout detectKeyboardLayout() noexcept {
    if (const auto fromPlatform = detail::detectFromPlatform(); fromPlatform.has_value()) {
        return *fromPlatform;
    }
    if (const auto fromEnv = detail::detectFromEnvironment(); fromEnv.has_value()) {
        return *fromEnv;
    }
    return KeyboardLayout::Qwerty;
}

const char* keyboardLayoutToString(KeyboardLayout layout) noexcept {
    switch (layout) {
        case KeyboardLayout::Qwerty:
            return "QWERTY";
        case KeyboardLayout::Azerty:
            return "AZERTY";
        default:
            return "UNKNOWN";
    }
}

}  // namespace input
