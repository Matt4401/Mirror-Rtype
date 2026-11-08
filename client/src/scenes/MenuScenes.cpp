#include "scenes/MenuScenes.hpp"

#include <charconv>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/UiEvents.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/SessionEvents.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneManager.hpp"
#include "scenes/ClientSettings.hpp"
#include "scenes/LobbyScenes.hpp"
#include "scenes/MenuKit.hpp"
#include "ui/components/UiTextInput.hpp"

namespace client {

namespace {

/** @brief Characters of a host name or an IP address. */
constexpr std::string_view HOST_CHARACTERS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-:";

/**
 * @brief Gets the settings of the client, if the settings plugin is there.
 * @param context Scene context.
 * @return The settings, or null.
 */
config::Settings* settingsOf(scene::SceneContext& context) {
    auto service = context.engine().mediator().getService<config::Settings>();
    return service ? &service->get() : nullptr;
}

/**
 * @brief Reads a port number.
 * @param text Text of the field.
 * @return The port, or 0 if it is not in [1, 65535].
 */
std::uint16_t parsePort(std::string_view text) {
    unsigned value = 0;
    const char* last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(std::to_address(text.begin()), last, value);
    if (error != std::errc{} || end != last || value == 0 || value > 65535U) {
        return 0;
    }
    return static_cast<std::uint16_t>(value);
}

}  // namespace

// --- Title -----------------------------------------------------------------------------------------------------------

void TitleScene::onEnter(scene::SceneContext& context) {
    const ecs::Entity box = menu::panel(context, 460.0F, 380.0F);
    (void)menu::label(context, box, "R-TYPE", 36.0F, 64.0F, menu::GOLD);
    (void)menu::label(context, box, "MIRROR ENGINE", 112.0F, 18.0F, menu::MUTED);
    _play = menu::button(context, box, "PLAY", "title.play", 170.0F, 0);
    (void)menu::button(context, box, "SETTINGS", "title.settings", 225.0F, 1);
    (void)menu::button(context, box, "QUIT", "title.quit", 280.0F, 2);
    menu::focus(context.registry(), _play);

    menu::onClick(context, [&context](std::string_view action) {
        if (action == "title.play") {
            context.manager().switchTo<ConnectScene>();
        } else if (action == "title.settings") {
            context.manager().push<SettingsScene>();
        } else if (action == "title.quit") {
            context.engine().stop();
        }
    });
}

void TitleScene::onResume(scene::SceneContext& context) { menu::focus(context.registry(), _play); }

// --- Connect ---------------------------------------------------------------------------------------------------------

ConnectScene::ConnectScene(std::string host, std::uint16_t port, bool preferDefaults)
    : _host(std::move(host)), _port(port), _preferDefaults(preferDefaults) {}

void ConnectScene::onEnter(scene::SceneContext& context) {
    std::string host = _host;
    std::uint16_t port = _port;
    if (const config::Settings* settings = settingsOf(context); settings != nullptr && !_preferDefaults) {
        if (!settings->getString(settings_keys::LAST_HOST).empty()) {
            host = settings->getString(settings_keys::LAST_HOST);
            port = static_cast<std::uint16_t>(settings->getInteger(settings_keys::LAST_PORT));
        }
    }

    const ecs::Entity box = menu::panel(context, 460.0F, 420.0F);
    (void)menu::label(context, box, "CONNECT TO A SERVER", 28.0F, 30.0F, menu::GOLD);
    _hostField =
        menu::textInput(context, box, "HOST", host, "connect.submit", std::string(HOST_CHARACTERS), 64, 120.0F, 0);
    _portField =
        menu::textInput(context, box, "PORT", std::to_string(port), "connect.submit", "0123456789", 5, 200.0F, 1);
    _connectButton = menu::button(context, box, "CONNECT", "connect.go", 260.0F, 2);
    (void)menu::button(context, box, "BACK", "connect.back", 315.0F, 3);
    const std::string& error = context.engine().getPlugin<RTypeNetworkPlugin>().lastError();
    _status = menu::label(context, box, error.empty() ? "" : "ERROR: " + error, 375.0F, 18.0F, menu::ERROR);
    menu::focus(context.registry(), _connectButton);

    const auto back = [&context] {
        if (context.engine().getPlugin<RTypeNetworkPlugin>().state() == SessionState::Connecting) {
            context.bus().publish(DisconnectRequest{});
        }
        context.manager().switchTo<TitleScene>();
    };
    menu::onBack(context, back);
    menu::onClick(context, [this, &context, back](std::string_view action) {
        if (action == "connect.go") {
            connect(context);
        } else if (action == "connect.back") {
            back();
        }
    });
    context.subscribe<ui::UiTextSubmittedEvent>([this, &context](const ui::UiTextSubmittedEvent& event) {
        if (event.action == "connect.submit" && context.manager().top() == context.id()) {
            connect(context);
        }
    });
    context.subscribe<SessionConnected>([this, &context](const SessionConnected& /*event*/) {
        ecs::Registry& registry = context.registry();
        if (config::Settings* settings = settingsOf(context); settings != nullptr) {
            settings->set(settings_keys::LAST_HOST, registry.getComponents<ui::UiTextInput>().get(_hostField).text);
            settings->set(settings_keys::LAST_PORT,
                          std::int64_t{parsePort(registry.getComponents<ui::UiTextInput>().get(_portField).text)});
        }
        context.manager().switchTo<LobbyScene>();
    });
    context.subscribe<SessionClosed>([this, &context](const SessionClosed& event) {
        menu::setText(context.registry(), _status, "ERROR: " + event.reason);
        menu::setColor(context.registry(), _status, menu::ERROR);
        menu::setText(context.registry(), _connectButton, "CONNECT");
    });
}

void ConnectScene::connect(scene::SceneContext& context) const {
    if (context.engine().getPlugin<RTypeNetworkPlugin>().state() != SessionState::Offline) {
        return;
    }
    ecs::Registry& registry = context.registry();
    const std::string& host = registry.getComponents<ui::UiTextInput>().get(_hostField).text;
    const std::uint16_t port = parsePort(registry.getComponents<ui::UiTextInput>().get(_portField).text);
    if (host.empty() || port == 0) {
        menu::setText(registry, _status, "ERROR: enter a host and a port between 1 and 65535");
        menu::setColor(registry, _status, menu::ERROR);
        return;
    }
    menu::setText(registry, _status, "CONNECTING TO " + host + ":" + std::to_string(port) + "...");
    menu::setColor(registry, _status, menu::MUTED);
    menu::setText(registry, _connectButton, "CONNECTING...");
    context.bus().publish(ConnectRequest{.host = host, .port = port});
}

// --- Settings --------------------------------------------------------------------------------------------------------

void SettingsScene::onEnter(scene::SceneContext& context) {
    config::Settings* settings = settingsOf(context);
    const auto number = [settings](std::string_view key) {
        return settings != nullptr ? static_cast<float>(settings->getNumber(key)) : 1.0F;
    };
    const ecs::Entity box = menu::panel(context, 500.0F, 400.0F);
    (void)menu::label(context, box, "SETTINGS", 28.0F, 34.0F, menu::GOLD);
    const ecs::Entity first =
        menu::slider(context, box, "MASTER", number(settings_keys::MASTER_VOLUME), "settings.master", 110.0F, 0);
    (void)menu::slider(context, box, "MUSIC", number(settings_keys::MUSIC_VOLUME), "settings.music", 160.0F, 1);
    (void)menu::slider(context, box, "EFFECTS", number(settings_keys::SFX_VOLUME), "settings.sfx", 210.0F, 2);
    (void)menu::toggle(context, box, "METRICS (F3)",
                       settings != nullptr && settings->getBool(settings_keys::SHOW_METRICS), "settings.metrics",
                       256.0F, 3);
    (void)menu::button(context, box, "BACK", "settings.back", 325.0F, 4);
    menu::focus(context.registry(), first);

    menu::onBack(context, [&context] { context.manager().pop(); });
    menu::onClick(context, [&context](std::string_view action) {
        if (action == "settings.back") {
            context.manager().pop();
        }
    });
    context.subscribe<ui::UiValueChangedEvent>([settings](const ui::UiValueChangedEvent& event) {
        if (settings == nullptr) {
            return;
        }
        const double value = event.value;
        if (event.action == "settings.master") {
            settings->set(settings_keys::MASTER_VOLUME, value);
        } else if (event.action == "settings.music") {
            settings->set(settings_keys::MUSIC_VOLUME, value);
        } else if (event.action == "settings.sfx") {
            settings->set(settings_keys::SFX_VOLUME, value);
        }
    });
    context.subscribe<ui::UiToggleChangedEvent>([settings](const ui::UiToggleChangedEvent& event) {
        if (settings != nullptr && event.action == "settings.metrics") {
            settings->set(settings_keys::SHOW_METRICS, event.on);
        }
    });
}

}  // namespace client
