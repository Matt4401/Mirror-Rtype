#pragma once

#include <string_view>

namespace ecs::net::detail {

/** @brief Tag of every log message from the network module: `[network] ...`. */
inline constexpr std::string_view LOG_TAG = "network";

}  // namespace ecs::net::detail
