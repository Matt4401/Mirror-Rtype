#pragma once

#include <cstdint>

namespace config {

/** @brief Largest configuration file accepted, so a corrupted or hostile file cannot exhaust memory. */
inline constexpr std::uintmax_t MAX_CONFIG_FILE_SIZE = std::uintmax_t{1024} * 1024;

}  // namespace config
