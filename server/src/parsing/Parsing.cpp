#include "Parsing.hpp"

#include <charconv>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace rtype::server {

namespace {

bool parsePort(const std::string_view arg, std::uint16_t& port) {
    const char* first = std::to_address(arg.begin());
    const char* last = std::to_address(arg.end());
    const auto [end, ec] = std::from_chars(first, last, port);
    return ec == std::errc{} && end == last && port != 0;
}

}  // namespace

std::optional<Options> parseOptions(const std::span<char* const> args) {
    Options options;
    bool portGiven = false;
    for (const std::string_view arg : args.subspan(1)) {
        if ((arg == LOGS_FLAG || arg == LOGS_COLORFUL_FLAG) && !options.logs) {
            options.logs = true;
            options.colors = arg == LOGS_COLORFUL_FLAG;
        } else if (!portGiven && parsePort(arg, options.port)) {
            portGiven = true;
        } else {
            return std::nullopt;
        }
    }
    return options;
}

std::string usage(const std::string_view programName) {
    return std::format("USAGE: {} [port] [{} | {}]", programName, LOGS_FLAG, LOGS_COLORFUL_FLAG);
}

}  // namespace rtype::server
