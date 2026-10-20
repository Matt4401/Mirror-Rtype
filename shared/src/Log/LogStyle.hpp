#pragma once

#include <string_view>

#include "Log/LogLevel.hpp"

/**
 * @brief ANSI styles of the colored logs (`--logs-colorful`).
 *
 * Severity uses green, yellow and red; tags use the other colors, so that the two never get mixed up.
 */
namespace rtype::log::style {

/** @brief ANSI code that turns every style off. */
inline constexpr std::string_view RESET = "\x1b[0m";
/** @brief ANSI code of the timestamp: dim. */
inline constexpr std::string_view TIME = "\x1b[2m";

/** @brief Style of the level name: the more severe, the more visible. */
[[nodiscard]] std::string_view level(Level level) noexcept;

/** @brief Style of the message text: debug fades out, warnings and errors stand out. */
[[nodiscard]] std::string_view message(Level level) noexcept;

/**
 * @brief Style of a tag: bold, in a color picked from the tag name.
 *
 * The color is a hash of the name, so a subsystem always gets the same color, and a new tag gets one without being
 * declared anywhere.
 */
[[nodiscard]] std::string_view tag(std::string_view tag) noexcept;

}  // namespace rtype::log::style
