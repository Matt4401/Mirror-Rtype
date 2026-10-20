#include "Log/LogStyle.hpp"

#include <array>
#include <cstdint>
#include <string_view>

#include "Log/LogLevel.hpp"

namespace rtype::log::style {

namespace {

constexpr std::uint32_t FNV1A_OFFSET_BASIS = 2166136261U;
constexpr std::uint32_t FNV1A_PRIME = 16777619U;

constexpr std::array<std::string_view, 6> TAG_PALETTE = {
    "\x1b[1;36m",  // bold cyan
    "\x1b[1;35m",  // bold magenta
    "\x1b[1;34m",  // bold blue
    "\x1b[1;96m",  // bold bright cyan
    "\x1b[1;95m",  // bold bright magenta
    "\x1b[1;94m",  // bold bright blue
};

}  // namespace

std::string_view level(const Level level) noexcept {
    switch (level) {
        case Level::Debug:
            return "\x1b[90m";  // gray
        case Level::Info:
            return "\x1b[1;32m";  // bold green
        case Level::Warning:
            return "\x1b[1;33m";  // bold yellow
        case Level::Error:
            return "\x1b[1;97;41m";  // bold white on red
    }
    return RESET;
}

std::string_view message(const Level level) noexcept {
    switch (level) {
        case Level::Debug:
            return "\x1b[2m";  // dim
        case Level::Info:
            return RESET;
        case Level::Warning:
            return "\x1b[33m";  // yellow
        case Level::Error:
            return "\x1b[1;31m";  // bold red
    }
    return RESET;
}

std::string_view tag(const std::string_view tag) noexcept {
    std::uint32_t hash = FNV1A_OFFSET_BASIS;
    for (const char c : tag) {
        hash = (hash ^ static_cast<std::uint8_t>(c)) * FNV1A_PRIME;
    }
    return TAG_PALETTE.at(hash % TAG_PALETTE.size());
}

}  // namespace rtype::log::style
