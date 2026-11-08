/**
 * @file ClientSettings.hpp
 * @brief Keys of the R-Type client settings, saved by `settings::SettingsPlugin`.
 */

#pragma once

#include <string_view>

namespace client::settings_keys {

/** @brief Master volume, [0, 1]. */
inline constexpr std::string_view MASTER_VOLUME = "audio.master";
/** @brief Music volume, [0, 1]. */
inline constexpr std::string_view MUSIC_VOLUME = "audio.music";
/** @brief Sound effects volume, [0, 1]. */
inline constexpr std::string_view SFX_VOLUME = "audio.sfx";
/** @brief Whether the metrics panel is shown. */
inline constexpr std::string_view SHOW_METRICS = "debug.metrics";
/** @brief Host of the last server the player connected to. */
inline constexpr std::string_view LAST_HOST = "net.last_host";
/** @brief Port of the last server the player connected to. */
inline constexpr std::string_view LAST_PORT = "net.last_port";

}  // namespace client::settings_keys
