#pragma once

#include <cstdint>
#include <string_view>

namespace rtype::log {

/** @brief Severity of a log message. */
enum class Level : std::uint8_t { Debug, Info, Warning, Error };

/** @brief Fixed-width name of a level, so that the messages stay aligned. */
constexpr std::string_view levelName(const Level level) noexcept {
    switch (level) {
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO ";
        case Level::Warning:
            return "WARN ";
        case Level::Error:
            return "ERROR";
    }
    return "?????";
}

}  // namespace rtype::log
