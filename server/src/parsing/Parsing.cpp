#include "Parsing.hpp"

#include <charconv>
#include <cstddef>
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
    const std::span<char* const> rest = args.empty() ? args : args.subspan(1);
    for (std::size_t i = 0; i < rest.size(); ++i) {
        const std::string_view arg = rest[i] != nullptr ? std::string_view(rest[i]) : std::string_view{};
        if (arg == LEVEL_OPTION) {
            if (options.level || i + 1 >= rest.size() || rest[i + 1] == nullptr || *rest[i + 1] == '\0') {
                return std::nullopt;
            }
            options.level = std::string(rest[++i]);
        } else if ((arg == LOGS_FLAG || arg == LOGS_COLORFUL_FLAG) && !options.logs) {
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
    return std::format("USAGE: {} [port] [{} | {}] [{} <path>]", programName, LOGS_FLAG, LOGS_COLORFUL_FLAG,
                       LEVEL_OPTION);
}

}  // namespace rtype::server
