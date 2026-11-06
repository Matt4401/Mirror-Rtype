#include "ClientOptions.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>

namespace client {

namespace {

/** @brief Reads a port number between 1 and 65535. */
bool parsePort(const std::string_view arg, std::uint16_t& port) {
    const char* const first = arg.data();
    const char* const last = std::next(first, static_cast<std::ptrdiff_t>(arg.size()));
    const auto [end, error] = std::from_chars(first, last, port);
    return error == std::errc{} && end == last && port != 0;
}

}  // namespace

std::optional<ClientOptions> parseOptions(const std::span<char* const> args) {
    const auto values = args.subspan(1);
    constexpr std::size_t MAX_VALUES = 2;
    if (values.size() > MAX_VALUES) {
        return std::nullopt;
    }
    ClientOptions options;
    if (!values.empty()) {
        options.host = values.front();
    }
    if (values.size() == MAX_VALUES && !parsePort(values.back(), options.port)) {
        return std::nullopt;
    }
    return options;
}

}  // namespace client
