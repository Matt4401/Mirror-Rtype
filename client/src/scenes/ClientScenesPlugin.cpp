#include "scenes/ClientScenesPlugin.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "audio/types/AudioTypes.hpp"
#include "components/Renderable.hpp"
#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IAudioService.hpp"
#include "core/services/IInputService.hpp"
#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "input/KeyNames.hpp"
#include "input/RTypeBindings.hpp"
#include "metrics/overlay/MetricsOverlayPlugin.hpp"
#include "net/PlayerControlPlugin.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "prefabs/PrefabManager.hpp"
#include "prefabs/PrefabParams.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "scene/SceneManager.hpp"
#include "scene/ScenePlugin.hpp"
#include "scene/SceneTypes.hpp"
#include "scene/scripting/SceneScriptingPlugin.hpp"
#include "scene/scripting/ScriptScene.hpp"
#include "scenes/ClientSettings.hpp"
#include "scenes/GameScenes.hpp"
#include "scenes/LobbyScenes.hpp"
#include "scenes/MenuKit.hpp"
#include "scenes/MenuScenes.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "settings/SettingsEvents.hpp"
#include "settings/SettingsPlugin.hpp"
#include "ui/UiPlugin.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"

namespace client {

namespace {

/** @brief Module of the scripted level transition, relative to the content root (the project root). */
constexpr const char* LEVEL_TRANSITION_MODULE = "scripts/scenes/LevelTransition";

/**
 * @brief Builds the `level_banner` prefab: a centred panel with the text of the `LevelBanner` resource.
 * @param entity Entity to build (it becomes the panel).
 * @param registry Registry of the entity.
 */
void buildLevelBanner(ecs::Entity entity, ecs::Registry& registry, const prefabs::PrefabParams& /*params*/) {
    const LevelBanner banner =
        registry.hasResource<LevelBanner>() ? registry.getResource<LevelBanner>() : LevelBanner{};
    registry.addComponent(entity, ui::UiAnchor{.anchorX = 0.5F, .anchorY = 0.4F, .pivotX = 0.5F, .pivotY = 0.5F});
    registry.addComponent(entity, ui::UiRect{.width = 520.0F,
                                             .height = 130.0F,
                                             .fillColor = menu::PANEL,
                                             .borderColor = menu::GOLD,
                                             .borderThickness = 2.0F});
    const auto line = [&](std::string text, float y, float size, ecs::Rgba color) {
        const ecs::Entity child = registry.spawnEntity();
        registry.addComponent(child, ui::UiAnchor{.anchorX = 0.5F, .offsetY = y, .pivotX = 0.5F});
        registry.addComponent(child, ui::UiText{.text = std::move(text), .fontSize = size, .color = color});
        // Destroyed with the banner when the overlay exits.
        hierarchy::setParent(registry, child, entity);
    };
    line(banner.title, 22.0F, 46.0F, menu::GOLD);
    line(banner.subtitle, 80.0F, 24.0F, menu::TEXT);
}

/**
 * @brief Gives the volume settings to the audio service, if there is one.
 * @param engine Host engine.
 * @param settings Client settings.
 */
void applyVolumes(engine::Engine& engine, const config::Settings& settings) {
    auto audio = engine.mediator().getService<engine::IAudioService>();
    if (!audio) {
        return;
    }
    audio->get().setVolume(audio::AudioBus::Master,
                           static_cast<float>(settings.getNumber(settings_keys::MASTER_VOLUME)));
    audio->get().setVolume(audio::AudioBus::Music, static_cast<float>(settings.getNumber(settings_keys::MUSIC_VOLUME)));
    audio->get().setVolume(audio::AudioBus::Sfx, static_cast<float>(settings.getNumber(settings_keys::SFX_VOLUME)));
}

/**
 * @brief Shows or hides the metrics panel as the settings say, if the program has one.
 * @param engine Host engine.
 * @param settings Settings.
 */
void applyMetrics(engine::Engine& engine, const config::Settings& settings) {
    if (engine.hasPlugin<metrics::MetricsOverlayPlugin>()) {
        engine.getPlugin<metrics::MetricsOverlayPlugin>().setVisible(settings.getBool(settings_keys::SHOW_METRICS));
    }
}

/**
 * @brief Reads the main key of each control from the settings; an unknown id keeps the default key.
 * @param settings Settings.
 * @return The keys.
 */
input::ControlKeys readControlKeys(const config::Settings& settings) {
    input::ControlKeys keys = input::defaultControlKeys();
    for (const input::Control control : input::CONTROLS) {
        if (const auto key = input::keyFromId(settings.getString(settings_keys::controlKey(control)))) {
            keys.at(static_cast<std::size_t>(control)) = *key;
        }
    }
    return keys;
}

/**
 * @brief Rebuilds the key bindings from the settings, if the program reads input.
 * @param engine Host engine.
 * @param settings Settings.
 */
void applyBindings(engine::Engine& engine, const config::Settings& settings) {
    if (auto inputService = engine.mediator().getService<engine::IInputService>()) {
        input::setupBindings(inputService->get().mapping(), readControlKeys(settings));
    }
}

/**
 * @brief Gives the UI scale setting to the menu kit; menus entered from now on use it.
 * @param engine Host engine.
 * @param settings Settings.
 */
void applyUiScale(engine::Engine& engine, const config::Settings& settings) {
    menu::setScale(engine.registry(), static_cast<float>(settings.getNumber(settings_keys::UI_SCALE)));
}

/**
 * @brief Gives the auto-fire setting to the player controls.
 * @param engine Host engine.
 * @param settings Settings.
 */
void applyAutoFire(engine::Engine& engine, const config::Settings& settings) {
    engine.getPlugin<PlayerControlPlugin>().setAutoFire(settings.getBool(settings_keys::AUTO_FIRE));
}

}  // namespace

ClientScenesPlugin::ClientScenesPlugin(std::string host, std::uint16_t port, bool fromCommandLine)
    : _host(std::move(host)), _port(port), _fromCommandLine(fromCommandLine) {}

void ClientScenesPlugin::build(engine::PluginBuilder& builder) {
    builder.require<scene::ScenePlugin>();
    builder.require<scene::SceneScriptingPlugin>();
    builder.require<scripting::ScriptPlugin>();
    builder.require<prefabs::PrefabPlugin>();
    builder.require<settings::SettingsPlugin>();
    builder.require<ui::UiPlugin>();
    builder.require<RTypeNetworkPlugin>();
    builder.require<PlayerControlPlugin>();
}

void ClientScenesPlugin::init(engine::Engine& engine) {
    config::Settings& settings = engine.getPlugin<settings::SettingsPlugin>().settings();
    settings.define(std::string(settings_keys::MASTER_VOLUME), 1.0);
    settings.define(std::string(settings_keys::MUSIC_VOLUME), 0.8);
    settings.define(std::string(settings_keys::SFX_VOLUME), 1.0);
    settings.define(std::string(settings_keys::SHOW_METRICS), false);
    settings.define(std::string(settings_keys::AUTO_FIRE), false);
    settings.define(std::string(settings_keys::UI_SCALE), 1.0);
    const input::ControlKeys defaults = input::defaultControlKeys();
    for (const input::Control control : input::CONTROLS) {
        settings.define(settings_keys::controlKey(control),
                        std::string(input::keyId(defaults.at(static_cast<std::size_t>(control)))));
    }
    settings.define(std::string(settings_keys::LAST_HOST), std::string{});
    settings.define(std::string(settings_keys::LAST_PORT), std::int64_t{_port});
    applyVolumes(engine, settings);
    applyMetrics(engine, settings);
    applyAutoFire(engine, settings);
    applyUiScale(engine, settings);
    applyBindings(engine, settings);
    engine.eventBus().subscribe<settings::SettingChanged>([&engine, &settings](const settings::SettingChanged& event) {
        if (event.key.starts_with("audio.")) {
            applyVolumes(engine, settings);
        } else if (event.key == settings_keys::SHOW_METRICS) {
            applyMetrics(engine, settings);
        } else if (event.key == settings_keys::AUTO_FIRE) {
            applyAutoFire(engine, settings);
        } else if (event.key == settings_keys::UI_SCALE) {
            applyUiScale(engine, settings);
        } else if (event.key.starts_with(settings_keys::CONTROLS_PREFIX)) {
            applyBindings(engine, settings);
        }
    });

    prefabs::PrefabManager& prefabs = engine.getPlugin<prefabs::PrefabPlugin>().manager();
    prefabs.registerPrefab("level_banner", &buildLevelBanner);
    engine.getPlugin<scripting::ScriptPlugin>().scriptSystem().setPrefabManager(&prefabs);

    scene::SceneManager& scenes = engine.getPlugin<scene::ScenePlugin>().manager();
    scenes.registerScene<TitleScene>();
    scenes.registerScene<ConnectScene>(_host, _port, _fromCommandLine);
    scenes.registerScene<SettingsScene>();
    scenes.registerScene<ControlsScene>();
    scenes.registerScene<LobbyScene>();
    scenes.registerScene<RoomScene>();
    scenes.registerScene<GameScene>();
    scenes.registerScene<PauseScene>();
    scenes.registerScene<GameOverScene>();
    engine.getPlugin<scene::SceneScriptingPlugin>().registerScriptScene(
        engine, std::string(LEVEL_TRANSITION_SCENE), LEVEL_TRANSITION_MODULE, "LevelTransition",
        scene::ScriptSceneOptions{.mode = scene::SceneMode::Overlay});
    scenes.switchTo<TitleScene>();
}

void ClientScenesPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    auto window = engine.mediator().getService<engine::IWindowService>();
    if (!window) {
        return;
    }
    const bool held = window->get().isKeyDown(engine::Key::F3);
    if (held && !_metricsKeyHeld) {
        config::Settings& settings = engine.getPlugin<settings::SettingsPlugin>().settings();
        settings.set(settings_keys::SHOW_METRICS, !settings.getBool(settings_keys::SHOW_METRICS));
    }
    _metricsKeyHeld = held;
}

}  // namespace client
