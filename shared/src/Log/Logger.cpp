#include "Log/Logger.hpp"

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>

#include "Log/LogLevel.hpp"
#include "Log/LogStyle.hpp"

namespace rtype::log {

namespace {

/** @brief Whether debug messages are printed. Off by default. */
std::atomic<bool>& verboseFlag() noexcept {
    static std::atomic<bool> flag{false};
    return flag;
}

/** @brief Whether the messages are colored with ANSI codes. Off by default. */
std::atomic<bool>& coloredFlag() noexcept {
    static std::atomic<bool> flag{false};
    return flag;
}

/** @brief Serializes the writes: the network thread and the game thread both log. */
std::mutex& outputMutex() noexcept {
    static std::mutex mutex;
    return mutex;
}

/** @brief Reference point of the timestamps, set during static initialization (program start). */
const std::chrono::steady_clock::time_point START_TIME = std::chrono::steady_clock::now();

std::string formatLine(const Level level, const std::string_view tag, const std::string_view message) {
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - START_TIME;
    if (!isColored()) {
        return std::format("[{:9.3f}] {} [{}] {}\n", elapsed.count(), levelName(level), tag, message);
    }
    return std::format("{}[{:9.3f}]{} {}{}{} {}[{}]{} {}{}{}\n", style::TIME, elapsed.count(), style::RESET,
                       style::level(level), levelName(level), style::RESET, style::tag(tag), tag, style::RESET,
                       style::message(level), message, style::RESET);
}

}  // namespace

void setVerbose(const bool enabled) noexcept { verboseFlag() = enabled; }

bool isVerbose() noexcept { return verboseFlag().load(); }

void setColored(const bool enabled) noexcept { coloredFlag() = enabled; }

bool isColored() noexcept { return coloredFlag().load(); }

namespace detail {

void write(const Level level, const std::string_view tag, const std::string_view message) {
    const std::string line = formatLine(level, tag, message);

    const std::scoped_lock lock(outputMutex());
    std::ostream& out = (level >= Level::Warning) ? std::cerr : std::cout;
    out << line << std::flush;
}

}  // namespace detail

}  // namespace rtype::log
