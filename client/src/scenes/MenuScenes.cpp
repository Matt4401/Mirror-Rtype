#include "scenes/MenuScenes.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "core/services/IWindowService.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "events/UiEvents.hpp"
#include "input/KeyNames.hpp"
#include "input/KeyboardLayout.hpp"
#include "input/RTypeBindings.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/SessionEvents.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneManager.hpp"
#include "scene/SceneTypes.hpp"
#include "scenes/ClientSettings.hpp"
#include "scenes/LobbyScenes.hpp"
#include "scenes/MenuKit.hpp"
#include "ui/components/UiTextInput.hpp"

namespace client {

namespace {

/** @brief Characters of a host name or an IP address. */
constexpr std::string_view HOST_CHARACTERS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-:";

/** @brief Width of the buttons of the settings and controls screens. */
constexpr float SETTINGS_BUTTON_WIDTH = 360.0F;
/** @brief Width of the buttons of the controls screen. */
constexpr float CONTROLS_BUTTON_WIDTH = 400.0F;
/** @brief Vertical distance between two controls. */
constexpr float CONTROLS_ROW_HEIGHT = 46.0F;
/** @brief Prefix of the action of a control's button, followed by its id. */
constexpr std::string_view CONTROL_ACTION_PREFIX = "controls.key.";
/** @brief Text sizes the settings cycle through, from the default. */
constexpr std::array<float, 4> SCALE_STEPS = {1.0F, 1.15F, 1.3F, 0.85F};

/**
 * @brief Gets the text size that follows `current` in `SCALE_STEPS`.
 * @param current Current scale.
 * @return The next step, or the first one if `current` is not a step.
 */
float nextScale(float current) noexcept {
    for (std::size_t i = 0; i < SCALE_STEPS.size(); ++i) {
        if (std::abs(SCALE_STEPS.at(i) - current) < 0.01F) {
            return SCALE_STEPS.at((i + 1) % SCALE_STEPS.size());
        }
    }
    return SCALE_STEPS.front();
}

/**
 * @brief Gets the label of the text size button.
 * @param scale Current scale.
 * @return E.g. "TEXT SIZE: 115%".
 */
std::string scaleLabel(float scale) {
    return "TEXT SIZE: " + std::to_string(static_cast<int>(std::lround(scale * 100.0F))) + "%";
}

/**
 * @brief Tells whether a key can be given to a control: Escape goes back and F3 shows the metrics.
 * @param key Key.
 * @return `false` for the keys the menus already use.
 */
bool isBindable(engine::Key key) noexcept { return key != engine::Key::Escape && key != engine::Key::F3; }

/**
 * @brief Gets the layout of the player's keyboard, detected once.
 * @return The layout.
 */
::input::KeyboardLayout layout() {
    static const ::input::KeyboardLayout detected = ::input::detectKeyboardLayout();
    return detected;
}

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
    (void)menu::label(context, box, "SQUACARON ENGINE", 112.0F, 18.0F, menu::MUTED);
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
    const auto flag = [settings](std::string_view key) { return settings != nullptr && settings->getBool(key); };
    const float scale = menu::scale(context.registry());
    const ecs::Entity box = menu::panel(context, 500.0F, 500.0F);
    (void)menu::label(context, box, "SETTINGS", 28.0F, 34.0F, menu::GOLD);
    const ecs::Entity first =
        menu::slider(context, box, "MASTER", number(settings_keys::MASTER_VOLUME), "settings.master", 95.0F, 0);
    (void)menu::slider(context, box, "MUSIC", number(settings_keys::MUSIC_VOLUME), "settings.music", 140.0F, 1);
    (void)menu::slider(context, box, "EFFECTS", number(settings_keys::SFX_VOLUME), "settings.sfx", 185.0F, 2);
    (void)menu::toggle(context, box, "METRICS (F3)", flag(settings_keys::SHOW_METRICS), "settings.metrics", 228.0F, 3);
    (void)menu::toggle(context, box, "AUTO-FIRE", flag(settings_keys::AUTO_FIRE), "settings.autofire", 270.0F, 4);
    const ecs::Entity scaleButton =
        menu::button(context, box, scaleLabel(scale), "settings.scale", 322.0F, 5, SETTINGS_BUTTON_WIDTH);
    (void)menu::button(context, box, "CONTROLS", "settings.controls", 377.0F, 6, SETTINGS_BUTTON_WIDTH);
    (void)menu::button(context, box, "BACK", "settings.back", 432.0F, 7, SETTINGS_BUTTON_WIDTH);
    menu::focus(context.registry(), _focusScale ? scaleButton : first);
    _focusScale = false;

    menu::onBack(context, [&context] { context.manager().pop(); });
    menu::onClick(context, [this, &context, settings, scale](std::string_view action) {
        if (action == "settings.back") {
            context.manager().pop();
        } else if (action == "settings.controls") {
            context.manager().push<ControlsScene>();
        } else if (action == "settings.scale" && settings != nullptr) {
            // The kit reads the scale when it spawns a widget: rebuild the scene to show the new size at once.
            settings->set(settings_keys::UI_SCALE, static_cast<double>(nextScale(scale)));
            _focusScale = true;
            context.manager().replace<SettingsScene>();
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
        if (settings == nullptr) {
            return;
        }
        if (event.action == "settings.metrics") {
            settings->set(settings_keys::SHOW_METRICS, event.on);
        } else if (event.action == "settings.autofire") {
            settings->set(settings_keys::AUTO_FIRE, event.on);
        }
    });
}

// --- Controls --------------------------------------------------------------------------------------------------------

void ControlsScene::onEnter(scene::SceneContext& context) {
    _waiting.reset();
    _wasDown.fill(true);  // A key held when the scene opens (Enter on "CONTROLS") is not a new press.
    _justBound = engine::Key::Unknown;

    const ecs::Entity box = menu::panel(context, 520.0F, 520.0F);
    (void)menu::label(context, box, "CONTROLS", 28.0F, 34.0F, menu::GOLD);
    int order = 0;
    float y = 90.0F;
    for (const input::Control control : input::CONTROLS) {
        _buttons.at(static_cast<std::size_t>(control)) =
            menu::button(context, box, "", std::string(CONTROL_ACTION_PREFIX).append(input::controlId(control)), y,
                         order++, CONTROLS_BUTTON_WIDTH);
        y += CONTROLS_ROW_HEIGHT;
    }
    (void)menu::button(context, box, "RESET", "controls.reset", y + 14.0F, order++, CONTROLS_BUTTON_WIDTH);
    (void)menu::button(context, box, "BACK", "controls.back", y + 60.0F, order, CONTROLS_BUTTON_WIDTH);
    (void)menu::label(context, box, "ESCAPE CANCELS - GAMEPAD BUTTONS ARE FIXED", y + 112.0F, 14.0F, menu::MUTED);
    refreshLabels(context);
    menu::focus(context.registry(), _buttons.front());

    menu::onBack(context, [this, &context] {
        if (_waiting) {
            _waiting.reset();
            refreshLabels(context);
        } else {
            context.manager().pop();
        }
    });
    menu::onClick(context, [this, &context](std::string_view action) {
        if (_waiting || _justBound != engine::Key::Unknown) {
            return;
        }
        if (action == "controls.back") {
            context.manager().pop();
            return;
        }
        config::Settings* settings = settingsOf(context);
        if (action == "controls.reset" && settings != nullptr) {
            for (const input::Control control : input::CONTROLS) {
                (void)settings->reset(settings_keys::controlKey(control));
            }
            refreshLabels(context);
            return;
        }
        for (const input::Control control : input::CONTROLS) {
            if (action.substr(CONTROL_ACTION_PREFIX.size()) == input::controlId(control) &&
                action.starts_with(CONTROL_ACTION_PREFIX)) {
                _waiting = control;
                // The key that clicked (Enter, Space) may still be down: only a press after this one counts.
                _wasDown.fill(true);
                menu::setText(context.registry(), _buttons.at(static_cast<std::size_t>(control)),
                              std::string(input::controlLabel(control)) + ": PRESS A KEY");
            }
        }
    });
    context.addSystem(scene::SceneStage::Update,
                      [this, &context](engine::Engine& /*engine*/, const ecs::time::Time&) { pollKeys(context); });
}

void ControlsScene::refreshLabels(scene::SceneContext& context) const {
    const config::Settings* settings = settingsOf(context);
    const input::ControlKeys defaults = input::defaultControlKeys();
    for (const input::Control control : input::CONTROLS) {
        const auto index = static_cast<std::size_t>(control);
        engine::Key key = defaults.at(index);
        if (settings != nullptr) {
            key = input::keyFromId(settings->getString(settings_keys::controlKey(control))).value_or(key);
        }
        menu::setText(context.registry(), _buttons.at(index),
                      std::string(input::controlLabel(control)) + ": " + input::keyLabel(key, layout()));
    }
}

void ControlsScene::pollKeys(scene::SceneContext& context) {
    auto window = context.engine().mediator().getService<engine::IWindowService>();
    if (!window) {
        return;
    }
    std::optional<engine::Key> pressed;
    for (const input::KeyName& name : input::keyNames()) {
        const bool down = window->get().isKeyDown(name.key);
        bool& wasDown = _wasDown.at(static_cast<std::size_t>(name.key));
        if (down && !wasDown && !pressed && isBindable(name.key)) {
            pressed = name.key;
        }
        wasDown = down;
    }
    if (_justBound != engine::Key::Unknown && !window->get().isKeyDown(_justBound)) {
        _justBound = engine::Key::Unknown;
    }
    if (!_waiting || !pressed) {
        return;
    }
    config::Settings* settings = settingsOf(context);
    if (settings != nullptr) {
        input::ControlKeys keys = input::defaultControlKeys();
        for (const input::Control control : input::CONTROLS) {
            const auto index = static_cast<std::size_t>(control);
            keys.at(index) =
                input::keyFromId(settings->getString(settings_keys::controlKey(control))).value_or(keys.at(index));
        }
        input::assignKey(keys, *_waiting, *pressed);
        for (const input::Control control : input::CONTROLS) {
            (void)settings->set(settings_keys::controlKey(control),
                                std::string(input::keyId(keys.at(static_cast<std::size_t>(control)))));
        }
    }
    // The arrows also move the focus: give it back to the control that was rebound.
    menu::focus(context.registry(), _buttons.at(static_cast<std::size_t>(*_waiting)));
    _waiting.reset();
    _justBound = *pressed;
    refreshLabels(context);
}

}  // namespace client
