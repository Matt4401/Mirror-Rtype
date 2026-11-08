#include "input/RTypeBindings.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

#include "core/services/InputCodes.hpp"
#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"

namespace client::input {

namespace {

/**
 * @brief Gets the slot of a control in `ControlKeys`.
 * @param control Control.
 * @return Its index.
 */
constexpr std::size_t slot(Control control) noexcept { return static_cast<std::size_t>(control); }

/**
 * @brief Gets the secondary key of a control.
 * @param control Control.
 * @return The key, or `engine::Key::Unknown` if it has none.
 */
constexpr engine::Key secondaryKey(Control control) noexcept {
    switch (control) {
        case Control::MoveUp:
            return engine::Key::Up;
        case Control::MoveDown:
            return engine::Key::Down;
        case Control::MoveLeft:
            return engine::Key::Left;
        case Control::MoveRight:
            return engine::Key::Right;
        case Control::Shoot:
            return engine::Key::Enter;
        case Control::Charge:
            return engine::Key::RightShift;
        case Control::Ready:
            return engine::Key::Unknown;
    }
    return engine::Key::Unknown;
}

/**
 * @brief Gets the secondary key of a control, unless a main key already uses it.
 * @param keys Main keys.
 * @param control Control.
 * @return The key, or `engine::Key::Unknown`.
 */
engine::Key freeSecondaryKey(const ControlKeys& keys, Control control) noexcept {
    const engine::Key key = secondaryKey(control);
    return std::ranges::contains(keys, key) ? engine::Key::Unknown : key;
}

/**
 * @brief Binds a button action to its main key, its secondary key and a gamepad button.
 * @param mapping Mapping to fill.
 * @param keys Main keys.
 * @param control Control of the action.
 * @param action Action name.
 * @param pad Gamepad button.
 */
void bindButton(::input::ActionMapping& mapping, const ControlKeys& keys, Control control, std::string_view action,
                engine::GamepadButton pad) {
    mapping.registerAction(action, ::input::ActionType::Button);
    mapping.bindKey(action, keys.at(slot(control)));
    if (const engine::Key extra = freeSecondaryKey(keys, control); extra != engine::Key::Unknown) {
        mapping.bindKey(action, extra);
    }
    mapping.bindGamepadButton(action, pad);
}

/**
 * @brief Binds an axis action to two pairs of keys (main, then secondary) and a gamepad stick axis.
 * @param mapping Mapping to fill.
 * @param keys Main keys.
 * @param negative Control of the negative side.
 * @param positive Control of the positive side.
 * @param action Action name.
 * @param axis Gamepad axis.
 */
void bindAxis(::input::ActionMapping& mapping, const ControlKeys& keys, Control negative, Control positive,
              std::string_view action, engine::GamepadAxis axis) {
    mapping.registerAction(action, ::input::ActionType::Axis);
    mapping.bindAxisComposite(action, keys.at(slot(negative)), keys.at(slot(positive)));
    mapping.bindAxisComposite(action, freeSecondaryKey(keys, negative), freeSecondaryKey(keys, positive));
    mapping.bindGamepadAxis(action, axis, 1.0F);
}

}  // namespace

std::string_view controlId(Control control) noexcept {
    switch (control) {
        case Control::MoveUp:
            return "move_up";
        case Control::MoveDown:
            return "move_down";
        case Control::MoveLeft:
            return "move_left";
        case Control::MoveRight:
            return "move_right";
        case Control::Shoot:
            return "shoot";
        case Control::Charge:
            return "charge";
        case Control::Ready:
            return "ready";
    }
    return "";
}

std::string_view controlLabel(Control control) noexcept {
    switch (control) {
        case Control::MoveUp:
            return "MOVE UP";
        case Control::MoveDown:
            return "MOVE DOWN";
        case Control::MoveLeft:
            return "MOVE LEFT";
        case Control::MoveRight:
            return "MOVE RIGHT";
        case Control::Shoot:
            return "SHOOT";
        case Control::Charge:
            return "CHARGE";
        case Control::Ready:
            return "READY";
    }
    return "";
}

ControlKeys defaultControlKeys() noexcept {
    ControlKeys keys{};
    keys.at(slot(Control::MoveUp)) = engine::Key::W;
    keys.at(slot(Control::MoveDown)) = engine::Key::S;
    keys.at(slot(Control::MoveLeft)) = engine::Key::A;
    keys.at(slot(Control::MoveRight)) = engine::Key::D;
    keys.at(slot(Control::Shoot)) = engine::Key::Space;
    keys.at(slot(Control::Charge)) = engine::Key::LeftShift;
    keys.at(slot(Control::Ready)) = engine::Key::R;
    return keys;
}

void assignKey(ControlKeys& keys, Control control, engine::Key key) noexcept {
    engine::Key& target = keys.at(slot(control));
    if (auto other = std::ranges::find(keys, key); other != keys.end()) {
        *other = target;
    }
    target = key;
}

void setupBindings(::input::ActionMapping& mapping, const ControlKeys& keys) {
    mapping.clear();
    bindAxis(mapping, keys, Control::MoveLeft, Control::MoveRight, "move_x", engine::GamepadAxis::LeftX);
    bindAxis(mapping, keys, Control::MoveUp, Control::MoveDown, "move_y", engine::GamepadAxis::LeftY);
    bindButton(mapping, keys, Control::Shoot, "shoot", engine::GamepadButton::RightFaceDown);
    bindButton(mapping, keys, Control::Charge, "charge_weapon", engine::GamepadButton::RightTrigger2);
    bindButton(mapping, keys, Control::Ready, "ready", engine::GamepadButton::MiddleRight);
}

void setupDefaultBindings(::input::ActionMapping& mapping) { setupBindings(mapping, defaultControlKeys()); }

}  // namespace client::input
