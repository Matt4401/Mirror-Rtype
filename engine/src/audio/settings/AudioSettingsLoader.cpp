#include "audio/settings/AudioSettingsLoader.hpp"

#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "audio/settings/AudioSettings.hpp"
#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "config/ConfigFile.hpp"
#include "config/ConfigReader.hpp"

namespace audio {

namespace {

constexpr std::string_view TABLE = "audio";
constexpr float MIN_PAN_RANGE = 1.0F;
constexpr float MAX_WORLD_DISTANCE = std::numeric_limits<float>::max();

/** @brief Reads the backend name, the only audio setting that is a string. */
AudioBackendKind readBackend(config::ConfigReader& reader, std::string_view key, AudioBackendKind fallback) {
    const std::string name = reader.text(key, "auto");
    if (name == "auto") {
        return AudioBackendKind::Auto;
    }
    if (name == "raylib") {
        return AudioBackendKind::Raylib;
    }
    if (name == "null") {
        return AudioBackendKind::Null;
    }
    reader.warn(std::format(R"({}: expected "auto", "raylib" or "null", using "auto")", key));
    return fallback;
}

}  // namespace

AudioSettings readAudioSettings(const config::ConfigFile& file, std::vector<std::string>& warnings) {
    config::ConfigReader reader(file, warnings);
    const AudioSettings defaults;
    AudioSettings settings;

    settings.backend = readBackend(reader, "audio.backend", defaults.backend);
    settings.voices.voicesPerSound =
        reader.count("audio.voices_per_sound", defaults.voices.voicesPerSound, 1, VOICES_PER_SOUND_LIMIT);
    settings.voices.maxActiveVoices =
        reader.count("audio.max_active_voices", defaults.voices.maxActiveVoices, 1, ACTIVE_VOICES_LIMIT);

    settings.masterVolume = reader.number("audio.volume.master", defaults.masterVolume, MIN_VOLUME, MAX_VOLUME);
    settings.musicVolume = reader.number("audio.volume.music", defaults.musicVolume, MIN_VOLUME, MAX_VOLUME);
    settings.sfxVolume = reader.number("audio.volume.sfx", defaults.sfxVolume, MIN_VOLUME, MAX_VOLUME);

    settings.spatial.enabled = reader.flag("audio.spatial.enabled", defaults.spatial.enabled);
    settings.spatial.panRange =
        reader.number("audio.spatial.pan_range", defaults.spatial.panRange, MIN_PAN_RANGE, MAX_WORLD_DISTANCE);
    settings.spatial.maxPan = reader.number("audio.spatial.max_pan", defaults.spatial.maxPan, PAN_CENTER, PAN_RIGHT);
    settings.spatial.minDistance =
        reader.number("audio.spatial.min_distance", defaults.spatial.minDistance, 0.0F, MAX_WORLD_DISTANCE);
    settings.spatial.maxDistance =
        reader.number("audio.spatial.max_distance", defaults.spatial.maxDistance, 0.0F, MAX_WORLD_DISTANCE);
    if (settings.spatial.maxDistance < settings.spatial.minDistance) {
        reader.warn(std::format("audio.spatial.max_distance: {} is below min_distance, using {}",
                                settings.spatial.maxDistance, settings.spatial.minDistance));
        settings.spatial.maxDistance = settings.spatial.minDistance;
    }
    settings.spatial.mergeDistance =
        reader.number("audio.spatial.merge_distance", defaults.spatial.mergeDistance, 0.0F, MAX_WORLD_DISTANCE);

    reader.reportUnknownKeys(TABLE);
    return settings;
}

}  // namespace audio
