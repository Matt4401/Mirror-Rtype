#include "input/RTypeBindings.hpp"

#include "core/services/InputCodes.hpp"
#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"

namespace client::input {

namespace {

void setupCommonActions(::input::ActionMapping& mapping) {
    mapping.registerAction("shoot", ::input::ActionType::Button);
    mapping.bindKey("shoot", engine::Key::Space);
    mapping.bindKey("shoot", engine::Key::Enter);
    mapping.bindGamepadButton("shoot", engine::GamepadButton::RightFaceDown);

    mapping.registerAction("charge_weapon", ::input::ActionType::Button);
    mapping.bindKey("charge_weapon", engine::Key::LeftShift);
    mapping.bindKey("charge_weapon", engine::Key::RightShift);
    mapping.bindGamepadButton("charge_weapon", engine::GamepadButton::RightTrigger2);
}

}  // namespace

void setupDefaultBindings(::input::ActionMapping& mapping) {
    mapping.clear();

    mapping.registerAction("move_x", ::input::ActionType::Axis);
    mapping.bindAxisComposite("move_x", engine::Key::A, engine::Key::D);
    mapping.bindAxisComposite("move_x", engine::Key::Left, engine::Key::Right);
    mapping.bindGamepadAxis("move_x", engine::GamepadAxis::LeftX, 1.0F);

    mapping.registerAction("move_y", ::input::ActionType::Axis);
    mapping.bindAxisComposite("move_y", engine::Key::W, engine::Key::S);
    mapping.bindAxisComposite("move_y", engine::Key::Up, engine::Key::Down);
    mapping.bindGamepadAxis("move_y", engine::GamepadAxis::LeftY, 1.0F);

    setupCommonActions(mapping);
}

}  // namespace client::input
