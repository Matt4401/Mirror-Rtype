/**
 * @file ClientSettings.hpp
 * @brief Keys of the R-Type client settings, saved by `settings::SettingsPlugin`.
 */

#pragma once

#include <string>
#include <string_view>

#include "input/RTypeBindings.hpp"

namespace client::settings_keys {

/** @brief Master volume, [0, 1]. */
inline constexpr std::string_view MASTER_VOLUME = "audio.master";
/** @brief Music volume, [0, 1]. */
inline constexpr std::string_view MUSIC_VOLUME = "audio.music";
/** @brief Sound effects volume, [0, 1]. */
inline constexpr std::string_view SFX_VOLUME = "audio.sfx";
/** @brief Whether the metrics panel is shown. */
inline constexpr std::string_view SHOW_METRICS = "debug.metrics";
/** @brief Whether one press of `shoot` starts firing and the next one stops it. */
inline constexpr std::string_view AUTO_FIRE = "access.auto_fire";
/** @brief Scale of the menus (text and widgets), in [`menu::MIN_SCALE`, `menu::MAX_SCALE`]. */
inline constexpr std::string_view UI_SCALE = "access.ui_scale";
/** @brief Prefix of the key bindings: `controls.<id>` holds the key id of a control (see `input::controlId`). */
inline constexpr std::string_view CONTROLS_PREFIX = "controls.";
/** @brief Host of the last server the player connected to. */
inline constexpr std::string_view LAST_HOST = "net.last_host";
/** @brief Port of the last server the player connected to. */
inline constexpr std::string_view LAST_PORT = "net.last_port";

/**
 * @brief Gets the settings key of a control's binding.
 * @param control Control.
 * @return "controls.<id>".
 */
[[nodiscard]] inline std::string controlKey(input::Control control) {
    return std::string(CONTROLS_PREFIX).append(input::controlId(control));
}

}  // namespace client::settings_keys
