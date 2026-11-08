#pragma once

#include <string_view>

namespace scripting::detail {

/** @brief Tag of every log message from the scripting module: `[script] ...`. */
inline constexpr std::string_view LOG_TAG = "script";

}  // namespace scripting::detail
