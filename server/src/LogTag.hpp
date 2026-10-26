#pragma once

#include <string_view>

namespace rtype::server {

/** @brief Tag of every log message from the server: `[server] ...`. */
inline constexpr std::string_view LOG_TAG = "server";

}  // namespace rtype::server
