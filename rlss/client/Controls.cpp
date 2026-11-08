#include "Controls.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include "config/Settings.hpp"
#include "core/services/InputCodes.hpp"
#include "game/Components.hpp"
#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"
#include "input/KeyNames.hpp"

namespace rlss::client {

namespace {

/**
 * @brief Gets the slot of a control in `ControlKeys`.
 * @param control Control.
 * @return Its index.
 */
constexpr std::size_t slot(Control control) noexcept { return static_cast<std::size_t>(control); }

/**
 * @brief Gets the second key of a control.
 * @param control Control.
 * @return The key, or `engine::Key::Unknown` if it has none.
 */
constexpr engine::Key secondaryKey(Control control) noexcept {
    switch (control) {
        case Control::SteerLeft:
            return engine::Key::Left;
        case Control::SteerRight:
            return engine::Key::Right;
        case Control::NoseUp:
            return engine::Key::Up;
        case Control::NoseDown:
            return engine::Key::Down;
        case Control::Jump:
            return engine::Key::Unknown;
        case Control::Boost:
            return engine::Key::RightShift;
    }
    return engine::Key::Unknown;
}

/**
 * @brief Gets the second key of a control, unless a main key already uses it.
 * @param keys Main keys.
 * @param control Control.
 * @return The key, or `engine::Key::Unknown`.
 */
engine::Key freeSecondaryKey(const ControlKeys& keys, Control control) noexcept {
    const engine::Key key = secondaryKey(control);
    return std::ranges::contains(keys, key) ? engine::Key::Unknown : key;
}

/**
 * @brief Binds a button action to the main key of a control and to its free second key.
 * @param mapping Mapping to fill.
 * @param keys Main keys.
 * @param control Control of the action.
 * @param action Action name.
 */
void bindButtonKeys(input::ActionMapping& mapping, const ControlKeys& keys, Control control, std::string_view action) {
    mapping.registerAction(action, input::ActionType::Button);
    mapping.bindKey(action, keys.at(slot(control)));
    if (const engine::Key extra = freeSecondaryKey(keys, control); extra != engine::Key::Unknown) {
        mapping.bindKey(action, extra);
    }
}

/**
 * @brief Binds an axis action to the main keys of two controls, then to their free second keys.
 * @param mapping Mapping to fill.
 * @param keys Main keys.
 * @param negative Control of the negative side.
 * @param positive Control of the positive side.
 * @param action Action name.
 */
void bindAxisKeys(input::ActionMapping& mapping, const ControlKeys& keys, Control negative, Control positive,
                  std::string_view action) {
    mapping.registerAction(action, input::ActionType::Axis);
    mapping.bindAxisComposite(action, keys.at(slot(negative)), keys.at(slot(positive)));
    mapping.bindAxisComposite(action, freeSecondaryKey(keys, negative), freeSecondaryKey(keys, positive));
}

}  // namespace

std::string_view controlId(Control control) noexcept {
    switch (control) {
        case Control::SteerLeft:
            return "steer_left";
        case Control::SteerRight:
            return "steer_right";
        case Control::NoseUp:
            return "nose_up";
        case Control::NoseDown:
            return "nose_down";
        case Control::Jump:
            return "jump";
        case Control::Boost:
            return "boost";
    }
    return "";
}

std::string_view controlLabel(Control control) noexcept {
    switch (control) {
        case Control::SteerLeft:
            return "GAUCHE";
        case Control::SteerRight:
            return "DROITE";
        case Control::NoseUp:
            return "NEZ EN HAUT";
        case Control::NoseDown:
            return "NEZ EN BAS";
        case Control::Jump:
            return "SAUT";
        case Control::Boost:
            return "BOOST";
    }
    return "";
}

std::string controlSetting(Control control) { return std::string(CONTROLS_PREFIX).append(controlId(control)); }

ControlKeys defaultControlKeys() noexcept {
    ControlKeys keys{};
    keys.at(slot(Control::SteerLeft)) = engine::Key::A;
    keys.at(slot(Control::SteerRight)) = engine::Key::D;
    keys.at(slot(Control::NoseUp)) = engine::Key::W;
    keys.at(slot(Control::NoseDown)) = engine::Key::S;
    keys.at(slot(Control::Jump)) = engine::Key::Space;
    keys.at(slot(Control::Boost)) = engine::Key::LeftShift;
    return keys;
}

void assignKey(ControlKeys& keys, Control control, engine::Key key) noexcept {
    engine::Key& target = keys.at(slot(control));
    if (auto other = std::ranges::find(keys, key); other != keys.end()) {
        *other = target;
    }
    target = key;
}

void defineControlSettings(config::Settings& settings) {
    const ControlKeys defaults = defaultControlKeys();
    for (const Control control : CONTROLS) {
        settings.define(controlSetting(control), std::string(input::keyId(defaults.at(slot(control)))));
    }
}

ControlKeys readControlKeys(const config::Settings& settings) {
    ControlKeys keys = defaultControlKeys();
    for (const Control control : CONTROLS) {
        const std::string key = controlSetting(control);
        if (!settings.has(key)) {
            continue;
        }
        if (const auto saved = input::keyFromId(settings.getString(key))) {
            keys.at(slot(control)) = *saved;
        }
    }
    return keys;
}

void writeControlKeys(config::Settings& settings, const ControlKeys& keys) {
    for (const Control control : CONTROLS) {
        (void)settings.set(controlSetting(control), std::string(input::keyId(keys.at(slot(control)))));
    }
}

void setupBindings(input::ActionMapping& mapping, const ControlKeys& keys) {
    mapping.clear();
    bindAxisKeys(mapping, keys, Control::SteerLeft, Control::SteerRight, game::ACTION_STEER);
    mapping.bindGamepadAxis(game::ACTION_STEER, engine::GamepadAxis::LeftX, 1.0F);

    bindAxisKeys(mapping, keys, Control::NoseDown, Control::NoseUp, game::ACTION_PITCH);
    mapping.bindGamepadAxis(game::ACTION_PITCH, engine::GamepadAxis::LeftY, -1.0F);  // the stick is negative up

    bindButtonKeys(mapping, keys, Control::Jump, game::ACTION_JUMP);
    mapping.bindGamepadButton(game::ACTION_JUMP, engine::GamepadButton::RightFaceDown);

    bindButtonKeys(mapping, keys, Control::Boost, game::ACTION_BOOST);
    mapping.bindGamepadButton(game::ACTION_BOOST, engine::GamepadButton::RightFaceRight);
    mapping.bindGamepadButton(game::ACTION_BOOST, engine::GamepadButton::RightTrigger2);
}

input::ActionMapping makeControls() {
    input::ActionMapping mapping;
    setupBindings(mapping, defaultControlKeys());
    return mapping;
}

}  // namespace rlss::client
