#pragma once

// Template definitions of Logger.hpp: included at its end only, never on its own.

#include <sstream>
#include <string_view>

#include "Log/LogLevel.hpp"

namespace rtype::log {

template <typename... Args>
void message(const Level level, const std::string_view tag, const Args&... args) {
    if (level == Level::Debug && !isVerbose()) {
        return;
    }
    std::ostringstream stream;
    // String literals decay to `const char*` here, as with any `operator<<` call.
    (stream << ... << args);  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    detail::write(level, tag, stream.view());
}

template <typename... Args>
void debug(const std::string_view tag, const Args&... args) {
    message(Level::Debug, tag, args...);
}

template <typename... Args>
void info(const std::string_view tag, const Args&... args) {
    message(Level::Info, tag, args...);
}

template <typename... Args>
void warning(const std::string_view tag, const Args&... args) {
    message(Level::Warning, tag, args...);
}

template <typename... Args>
void error(const std::string_view tag, const Args&... args) {
    message(Level::Error, tag, args...);
}

}  // namespace rtype::log
