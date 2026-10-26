#pragma once

#include <string>
#include <vector>

#include "audio/settings/AudioSettings.hpp"
#include "config/ConfigFile.hpp"

namespace audio {

/**
 * @brief Reads the `[audio]` table of a configuration file.
 *
 * Never fails: missing keys keep their default, and invalid values (wrong type, out of range, unknown backend)
 * are replaced or clamped and reported in `warnings`, as are unknown keys. Recognized keys:
 *
 * @code{.toml}
 * [audio]
 * backend = "auto"            # "auto", "raylib" or "null"
 * voices_per_sound = 4
 * max_active_voices = 32
 *
 * [audio.volume]
 * master = 1.0
 * music = 1.0
 * sfx = 1.0
 *
 * [audio.spatial]
 * enabled = true
 * pan_range = 640.0
 * max_pan = 0.6
 * min_distance = 750.0
 * max_distance = 1500.0
 * @endcode
 *
 * @param file Parsed configuration; an empty one gives the default settings.
 * @param warnings Receives one message per ignored or corrected value.
 * @return The settings to use.
 */
[[nodiscard]] AudioSettings readAudioSettings(const config::ConfigFile& file, std::vector<std::string>& warnings);

}  // namespace audio
