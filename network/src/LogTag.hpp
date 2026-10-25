#pragma once

#include <string_view>

namespace rtype::net {

/** @brief Tag of every log message from the network module: `[network] ...`. */
inline constexpr std::string_view LOG_TAG = "network";

}  // namespace rtype::net
