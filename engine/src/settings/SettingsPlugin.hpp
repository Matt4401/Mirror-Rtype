/**
 * @file SettingsPlugin.hpp
 * @brief Plugin owning the user settings: loaded at start-up, saved shortly after a change and at shutdown.
 */

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "config/Settings.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"

namespace settings {

/**
 * @brief Owns the `config::Settings` of the game.
 *
 * - The settings are a Mediator service (`engine.service<config::Settings>()`); declare them through `settings()`
 *   before or after `Engine::init`.
 * - `init` loads the file. Values that differ from the defaults are reported as changes, so the plugins reacting to
 *   `SettingChanged` apply the saved values on the first frame.
 * - `update` publishes one `SettingChanged` per changed key, and saves once no setting changed for `saveDelay`
 *   seconds (a dragged volume slider writes the file once, not every frame).
 * - `shutdown` saves pending changes.
 *
 * Load and save errors are kept in `lastError()` and never stop the game: it runs with the defaults.
 */
class SettingsPlugin final : public engine::IPlugin {
  public:
    /** @brief Default quiet time before a save, in seconds. */
    static constexpr float DEFAULT_SAVE_DELAY = 1.0F;

    /**
     * @brief Creates the plugin.
     * @param file Settings file (see `config::userConfigDirectory`); empty to keep the settings in memory only.
     * @param saveDelay Quiet time before a save, in seconds.
     */
    explicit SettingsPlugin(std::filesystem::path file, float saveDelay = DEFAULT_SAVE_DELAY);

    /**
     * @brief Gets plugin name.
     * @return String view "SettingsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "SettingsPlugin"; }

    /**
     * @brief Declares the core plugin and the settings service.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Loads the file and registers the settings in the Mediator.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Publishes the changes and saves after the quiet time.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Saves pending changes and removes the service.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Gets the settings.
     * @return The settings, usable before `init` to declare keys.
     */
    [[nodiscard]] config::Settings& settings() noexcept { return *_settings; }

    /**
     * @brief Gets the last load or save error.
     * @return Error message, or empty.
     */
    [[nodiscard]] const std::string& lastError() const noexcept { return _lastError; }

  private:
    /** @brief Saves, keeping the error if any. */
    void save();

    /** @brief The settings, shared with the Mediator. */
    std::shared_ptr<config::Settings> _settings;
    /** @brief Quiet time before a save. */
    float _saveDelay;
    /** @brief Time since the last change, while a save is due. */
    float _sinceChange = 0.0F;
    /** @brief Last load or save error. */
    std::string _lastError;
};

}  // namespace settings
