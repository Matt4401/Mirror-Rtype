/**
 * @file VolumeCVarsPlugin.hpp
 * @brief Console variables for the volume settings of the RLSS client.
 */

#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "tools/cvars/CVarRef.hpp"

namespace rlss::client {

/**
 * @brief Exposes the volume settings as cvars: `snd_master`, `snd_music`, `snd_sfx` (0 to 1).
 *
 * The cvars and the settings stay in sync both ways, through events: a console change sets the setting (so the volume
 * applies and is saved), and a change from the options menu sets the cvar. The cvars are archived: the settings file
 * keeps them between sessions.
 */
class VolumeCVarsPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "VolumeCVarsPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "VolumeCVarsPlugin"; }

    /**
     * @brief Declares the settings, the console, and the menus that declare the volume settings.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Registers the cvars at the current setting values and subscribes to both change events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Unsubscribes from the change events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief One volume: its cvar and its setting. */
    struct Volume {
        /** @brief Cvar name. */
        std::string_view cvar;
        /** @brief Setting key. */
        std::string_view setting;
        /** @brief Help text of the cvar. */
        std::string_view description;
        /** @brief Handle on the cvar, once registered. */
        tools::CVarRef<float> ref;
    };

    /** @brief The three volumes. */
    std::array<Volume, 3> _volumes{
        {
            {.cvar = "snd_master", .setting = "audio.master", .description = "Master volume (0 to 1)", .ref = {}},
            {.cvar = "snd_music", .setting = "audio.music", .description = "Music volume (0 to 1)", .ref = {}},
            {.cvar = "snd_sfx", .setting = "audio.sfx", .description = "Sound effects volume (0 to 1)", .ref = {}},
        },
    };
    /** @brief Subscription to the cvar changes. */
    std::optional<ecs::EventBus::EventToken> _cvarChanged;
    /** @brief Subscription to the setting changes. */
    std::optional<ecs::EventBus::EventToken> _settingChanged;
};

}  // namespace rlss::client
