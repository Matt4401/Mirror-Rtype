/**
 * @file SettingsEvents.hpp
 * @brief Event published when a user setting changes.
 */

#pragma once

#include <string>

namespace settings {

/**
 * @brief A setting changed (by `config::Settings::set`, or by the file loaded at start-up).
 *
 * Published once per changed key, at the next `SettingsPlugin::update`. Read the new value from the settings.
 */
struct SettingChanged {
    /** @brief Dotted key of the setting. */
    std::string key;
};

}  // namespace settings
