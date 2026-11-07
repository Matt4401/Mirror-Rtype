#include "CommandLine.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <system_error>

namespace rlss {

std::optional<std::uint16_t> parsePort(const std::string_view arg) {
    std::uint16_t port = 0;
    const char* const first = arg.data();
    const char* const last = std::next(first, static_cast<std::ptrdiff_t>(arg.size()));
    const auto [end, error] = std::from_chars(first, last, port);
    if (error != std::errc{} || end != last || port == 0) {
        return std::nullopt;
    }
    return port;
}

}  // namespace rlss
