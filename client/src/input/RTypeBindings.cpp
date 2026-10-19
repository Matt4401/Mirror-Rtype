#include "input/RTypeBindings.hpp"

#include <raylib.h>

#include "input/ActionMapping.hpp"
#include "input/InputAction.hpp"

namespace client::input {

void setupDefaultBindings(::input::ActionMapping& mapping) {
    mapping.clear();

    // MoveUp (AZERTY Z, QWERTY W, Arrow Up, D-Pad Up, Left Stick Up)
    mapping.bindKey(::input::Action::MoveUp, KEY_W);
    mapping.bindKey(::input::Action::MoveUp, KEY_Z);
    mapping.bindKey(::input::Action::MoveUp, KEY_UP);
    mapping.bindGamepadButton(::input::Action::MoveUp, GAMEPAD_BUTTON_LEFT_FACE_UP);
    mapping.bindGamepadAxis(::input::Action::MoveUp, GAMEPAD_AXIS_LEFT_Y, -1.0F);

    // MoveDown (S, Arrow Down, D-Pad Down, Left Stick Down)
    mapping.bindKey(::input::Action::MoveDown, KEY_S);
    mapping.bindKey(::input::Action::MoveDown, KEY_DOWN);
    mapping.bindGamepadButton(::input::Action::MoveDown, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
    mapping.bindGamepadAxis(::input::Action::MoveDown, GAMEPAD_AXIS_LEFT_Y, 1.0F);

    // MoveLeft (AZERTY Q, QWERTY A, Arrow Left, D-Pad Left, Left Stick Left)
    mapping.bindKey(::input::Action::MoveLeft, KEY_A);
    mapping.bindKey(::input::Action::MoveLeft, KEY_Q);
    mapping.bindKey(::input::Action::MoveLeft, KEY_LEFT);
    mapping.bindGamepadButton(::input::Action::MoveLeft, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    mapping.bindGamepadAxis(::input::Action::MoveLeft, GAMEPAD_AXIS_LEFT_X, -1.0F);

    // MoveRight (D, Arrow Right, D-Pad Right, Left Stick Right)
    mapping.bindKey(::input::Action::MoveRight, KEY_D);
    mapping.bindKey(::input::Action::MoveRight, KEY_RIGHT);
    mapping.bindGamepadButton(::input::Action::MoveRight, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    mapping.bindGamepadAxis(::input::Action::MoveRight, GAMEPAD_AXIS_LEFT_X, 1.0F);

    // Shoot (Space, Enter, Gamepad A / Cross)
    mapping.bindKey(::input::Action::Shoot, KEY_SPACE);
    mapping.bindKey(::input::Action::Shoot, KEY_ENTER);
    mapping.bindGamepadButton(::input::Action::Shoot, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);

    // ChargeWeapon (Shift, Gamepad Right Trigger)
    mapping.bindKey(::input::Action::ChargeWeapon, KEY_LEFT_SHIFT);
    mapping.bindKey(::input::Action::ChargeWeapon, KEY_RIGHT_SHIFT);
    mapping.bindGamepadButton(::input::Action::ChargeWeapon, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
}

}  // namespace client::input
