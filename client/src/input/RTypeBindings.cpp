#include "input/RTypeBindings.hpp"

#include <raylib.h>

#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"

namespace client::input {

namespace {

void setupCommonActions(::input::ActionMapping& mapping) {
    mapping.registerAction("shoot", ::input::ActionType::Button);
    mapping.bindKey("shoot", KEY_SPACE);
    mapping.bindKey("shoot", KEY_ENTER);
    mapping.bindGamepadButton("shoot", GAMEPAD_BUTTON_RIGHT_FACE_DOWN);

    mapping.registerAction("charge_weapon", ::input::ActionType::Button);
    mapping.bindKey("charge_weapon", KEY_LEFT_SHIFT);
    mapping.bindKey("charge_weapon", KEY_RIGHT_SHIFT);
    mapping.bindGamepadButton("charge_weapon", GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
}

}  // namespace

void setupDefaultBindings(::input::ActionMapping& mapping) {
    mapping.clear();

    mapping.registerAction("move_x", ::input::ActionType::Axis);
    mapping.bindAxisComposite("move_x", KEY_A, KEY_D);
    mapping.bindAxisComposite("move_x", KEY_LEFT, KEY_RIGHT);
    mapping.bindGamepadAxis("move_x", GAMEPAD_AXIS_LEFT_X, 1.0F);

    mapping.registerAction("move_y", ::input::ActionType::Axis);
    mapping.bindAxisComposite("move_y", KEY_W, KEY_S);
    mapping.bindAxisComposite("move_y", KEY_UP, KEY_DOWN);
    mapping.bindGamepadAxis("move_y", GAMEPAD_AXIS_LEFT_Y, 1.0F);

    setupCommonActions(mapping);
}

}  // namespace client::input
