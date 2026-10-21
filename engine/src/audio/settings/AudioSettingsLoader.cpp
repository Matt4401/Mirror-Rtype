#include "audio/settings/AudioSettingsLoader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "audio/common/AudioConstants.hpp"
#include "audio/common/AudioTypes.hpp"
#include "audio/settings/AudioSettings.hpp"
#include "config/ConfigFile.hpp"

namespace audio {

namespace {

constexpr std::string_view TABLE = "audio";
constexpr float MIN_PAN_RANGE = 1.0F;
constexpr float MAX_WORLD_DISTANCE = std::numeric_limits<float>::max();

/** @brief Reads typed, bounded values from a configuration and reports every correction. */
class SettingsReader {
  public:
    SettingsReader(const config::ConfigFile& file, std::vector<std::string>& warnings)
        : _file(&file), _warnings(&warnings) {}

    /** @brief Reads a number clamped to [min, max]. */
    float number(std::string_view key, float fallback, float min, float max) {
        if (!present(key)) {
            return fallback;
        }
        const std::optional<double> value = _file->getNumber(key);
        if (!value || !std::isfinite(*value)) {
            warn(std::format("{}: expected a finite number, using {}", key, fallback));
            return fallback;
        }
        const double clamped = std::clamp(*value, static_cast<double>(min), static_cast<double>(max));
        if (clamped != *value) {
            warn(std::format("{}: {} is out of range [{}, {}], using {}", key, *value, min, max, clamped));
        }
        return static_cast<float>(clamped);
    }

    /** @brief Reads a count clamped to [min, max]. */
    std::size_t count(std::string_view key, std::size_t fallback, std::size_t min, std::size_t max) {
        if (!present(key)) {
            return fallback;
        }
        const std::optional<std::int64_t> value = _file->getInteger(key);
        if (!value) {
            warn(std::format("{}: expected an integer, using {}", key, fallback));
            return fallback;
        }
        const auto clamped = std::clamp(*value, static_cast<std::int64_t>(min), static_cast<std::int64_t>(max));
        if (clamped != *value) {
            warn(std::format("{}: {} is out of range [{}, {}], using {}", key, *value, min, max, clamped));
        }
        return static_cast<std::size_t>(clamped);
    }

    /** @brief Reads a boolean. */
    bool flag(std::string_view key, bool fallback) {
        if (!present(key)) {
            return fallback;
        }
        const std::optional<bool> value = _file->getBool(key);
        if (!value) {
            warn(std::format("{}: expected true or false, using {}", key, fallback));
            return fallback;
        }
        return *value;
    }

    /** @brief Reads the backend name. */
    AudioBackendKind backend(std::string_view key, AudioBackendKind fallback) {
        if (!present(key)) {
            return fallback;
        }
        const std::optional<std::string> name = _file->getString(key);
        if (name == "auto") {
            return AudioBackendKind::Auto;
        }
        if (name == "raylib") {
            return AudioBackendKind::Raylib;
        }
        if (name == "null") {
            return AudioBackendKind::Null;
        }
        warn(std::format(R"({}: expected "auto", "raylib" or "null", using "auto")", key));
        return fallback;
    }

    /** @brief Reports the keys of `table` that were never read. */
    void reportUnknownKeys(std::string_view table) {
        for (const std::string& key : _file->keysUnder(table)) {
            if (!std::ranges::contains(_read, key)) {
                warn(std::format("{}: unknown setting, ignored", key));
            }
        }
    }

    /** @brief Adds a message to the warnings. */
    void warn(std::string message) { _warnings->push_back(std::move(message)); }

  private:
    /** @brief Records `key` as known and tells whether the file sets it. */
    bool present(std::string_view key) {
        _read.emplace_back(key);
        return _file->contains(key);
    }

    const config::ConfigFile* _file;
    std::vector<std::string>* _warnings;
    std::vector<std::string> _read;
};

}  // namespace

AudioSettings readAudioSettings(const config::ConfigFile& file, std::vector<std::string>& warnings) {
    SettingsReader reader(file, warnings);
    const AudioSettings defaults;
    AudioSettings settings;

    settings.backend = reader.backend("audio.backend", defaults.backend);
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

    reader.reportUnknownKeys(TABLE);
    return settings;
}

}  // namespace audio
